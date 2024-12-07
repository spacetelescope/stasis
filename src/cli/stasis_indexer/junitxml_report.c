//
// Created by jhunk on 11/15/24.
//

#include "core.h"
#include "callbacks.h"
#include "junitxml.h"
#include "junitxml_report.h"

int indexer_junitxml_report(struct Delivery ctx[], const size_t nelem) {
    struct Delivery *latest = get_latest_deliveries(ctx, nelem);
    if (!latest) {
        return -1;
    }
    size_t latest_count;
    ARRAY_COUNT_BY_STRUCT_MEMBER(latest, meta.name, latest_count);

    char indexfile[PATH_MAX] = {0};
    sprintf(indexfile, "%s/README.md", ctx->storage.results_dir);

    struct StrList *file_listing = listdir(ctx->storage.results_dir);
    if (!file_listing) {
        // no test results to process
        return 0;
    }

    if (!pushd(ctx->storage.results_dir)) {
        FILE *indexfp = fopen(indexfile, "w+");
        if (!indexfp) {
            fprintf(stderr, "Unable to open %s for writing\n", indexfile);
            return -1;
        }

        struct StrList *archs = get_architectures(latest, nelem);
        struct StrList *platforms = get_platforms(latest, nelem);
        qsort(latest, latest_count, sizeof(*latest), callback_sort_deliveries_dynamic_cmpfn);
        fprintf(indexfp, "# %s-%s Test Report\n\n", ctx->meta.name, ctx->meta.version);
        size_t no_printable_data = 0;

        size_t delivery_count = get_latest_rc(latest, latest_count);
            for (size_t p = 0; p < strlist_count(platforms); p++) {
                char *platform = strlist_item(platforms, p);
                for (size_t a = 0; a < strlist_count(archs); a++) {
                    char *arch = strlist_item(archs, a);

                    fprintf(indexfp, "## %s-%s\n\n", platform, arch);
                    for (size_t d = 0; d < delivery_count; d++) {
                        struct Delivery *current = &ctx[d];
                        if (current->meta.rc == (int) d + 1
                            && strcmp(current->system.arch, arch) != 0
                            && strcmp(current->system.platform[DELIVERY_PLATFORM_RELEASE], platform) != 0) {
                            continue;
                        }

                        fprintf(indexfp, "### %s\n", current->info.release_name);
                        fprintf(indexfp, "\n|Suite|Duration|Fail    |Skip |Error |\n");
                        fprintf(indexfp, "|:----|:------:|:------:|:---:|:----:|\n");
                        for (size_t f = 0; f < strlist_count(file_listing); f++) {
                            char *filename = strlist_item(file_listing, f);
                            if (!endswith(filename, ".xml")) {
                                continue;
                            }

                            char pattern[PATH_MAX] = {0};
                            snprintf(pattern, sizeof(pattern) - 1, "*%s*", current->info.release_name);
                            if (!fnmatch(pattern, filename, 0) && strstr(filename, platform) &&
                                strstr(filename, arch)) {
                                struct JUNIT_Testsuite *testsuite = junitxml_testsuite_read(filename);
                                if (testsuite) {
                                    if (globals.verbose) {
                                        printf("%s: duration: %0.4f, failed: %d, skipped: %d, errors: %d\n", filename,
                                               testsuite->time, testsuite->failures, testsuite->skipped,
                                               testsuite->errors);
                                    }

                                    char *bname_tmp = strdup(filename);
                                    char *bname = path_basename(bname_tmp);
                                    bname[strlen(bname) - 4] = 0;
                                    guard_free(bname_tmp);

                                    char result_outfile[PATH_MAX] = {0};

                                    char result_path[PATH_MAX] = {0};
                                    //snprintf(result_path, sizeof(result_path) -1 , "%s/%s", platform, arch);
                                    //mkdirs(result_path, 0755);

                                    char *short_name_pattern = NULL;
                                    asprintf(&short_name_pattern, "-%s", current->info.release_name);

                                    char short_name[PATH_MAX] = {0};
                                    strncpy(short_name, bname, sizeof(short_name) - 1);
                                    replace_text(short_name, short_name_pattern, "", 0);
                                    replace_text(short_name, "results-", "", 0);
                                    guard_free(short_name_pattern);

                                    fprintf(indexfp, "|[%s](%s.html)|%0.4f|%d|%d|%d|\n", short_name,
                                            bname,
                                            testsuite->time, testsuite->failures, testsuite->skipped,
                                            testsuite->errors);

                                    snprintf(result_outfile, sizeof(result_outfile) - strlen(bname) - 3, "%s.md",
                                             bname);
                                    FILE *resultfp = fopen(result_outfile, "w+");
                                    if (!resultfp) {
                                        SYSERROR("Unable to open %s for writing", result_outfile);
                                        return -1;
                                    }

                                    for (size_t i = 0; i < testsuite->_tc_inuse; i++) {
                                        if (testsuite->testcase[i]->tc_result_state_type) {
                                            const char *type_str = NULL;
                                            const int state = testsuite->testcase[i]->tc_result_state_type;
                                            const char *message = NULL;
                                            if (state == JUNIT_RESULT_STATE_FAILURE) {
                                                message = testsuite->testcase[i]->result_state.failure->message;
                                                type_str = "[FAILED]";
                                            } else if (state == JUNIT_RESULT_STATE_ERROR) {
                                                message = testsuite->testcase[i]->result_state.error->message;
                                                type_str = "[ERROR]";
                                            } else if (state == JUNIT_RESULT_STATE_SKIPPED) {
                                                message = testsuite->testcase[i]->result_state.skipped->message;
                                                type_str = "[SKIPPED]";
                                            }
                                            fprintf(resultfp, "### %s %s :: %s\n", type_str,
                                                    testsuite->testcase[i]->classname, testsuite->testcase[i]->name);
                                            fprintf(resultfp, "\nDuration: %0.04fs\n", testsuite->testcase[i]->time);
                                            fprintf(resultfp, "\n```\n%s\n```\n", message);
                                        }
                                    }
                                    junitxml_testsuite_free(&testsuite);
                                    fclose(resultfp);
                                } else {
                                    fprintf(stderr, "bad test suite: %s: %s\n", strerror(errno), filename);
                                }
                            } else {
                                if (!no_printable_data) {
                                    // Triggering for reasons unknown
                                    //fprintf(indexfp, "|No data|-|-|-|-|-|-|\n");
                                    no_printable_data = 1;
                                }
                            }
                        }
                    }
                    fprintf(indexfp, "\n");
                    no_printable_data = 0;
                }
                fprintf(indexfp, "\n");
        }
        guard_strlist_free(&archs);
        guard_strlist_free(&platforms);
        fclose(indexfp);
        popd();
    } else {
        fprintf(stderr, "Unable to enter delivery directory: %s\n", ctx->storage.delivery_dir);
        guard_free(latest);
        return -1;
    }

    // "latest" is an array of pointers to ctxs[]. Do not free the contents of the array.
    guard_free(latest);
    return 0;
}


