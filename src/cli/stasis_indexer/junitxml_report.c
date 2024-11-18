//
// Created by jhunk on 11/15/24.
//

#include "core.h"
#include "callbacks.h"
#include "junitxml.h"
#include "junitxml_report.h"

int indexer_junitxml_report(struct Delivery ctx[], const size_t nelem) {
    struct Delivery **latest = NULL;
    latest = get_latest_deliveries(ctx, nelem);

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
        struct StrList *archs = get_architectures(*latest, nelem);
        struct StrList *platforms = get_platforms(*latest, nelem);

        qsort(latest, nelem, sizeof(*latest), callback_sort_deliveries_dynamic_cmpfn);
        fprintf(indexfp, "# %s-%s Test Report\n\n", ctx->meta.name, ctx->meta.version);
        fprintf(indexfp, "## Current Release\n\n");
        size_t no_printable_data = 0;
        for (size_t p = 0; p < strlist_count(platforms); p++) {
            char *platform = strlist_item(platforms, p);
            for (size_t a = 0; a < strlist_count(archs); a++) {
                char *arch = strlist_item(archs, a);
                int have_combo = 0;
                for (size_t i = 0; i < nelem; i++) {
                    if (latest[i] && latest[i]->system.platform) {
                        if (strstr(latest[i]->system.platform[DELIVERY_PLATFORM_RELEASE], platform) &&
                            strstr(latest[i]->system.arch, arch)) {
                            have_combo = 1;
                            break;
                        }
                    }
                }
                if (!have_combo) {
                    continue;
                }
                fprintf(indexfp, "### %s-%s\n\n", platform, arch);

                fprintf(indexfp, "|Suite|Duration|Fail    |Skip |Error |\n");
                fprintf(indexfp, "|:----|:------:|:------:|:---:|:----:|\n");
                for (size_t f = 0; f < strlist_count(file_listing); f++) {
                    char *filename = strlist_item(file_listing, f);
                    if (!endswith(filename, ".xml")) {
                        continue;
                    }

                    if (strstr(filename, platform) && strstr(filename, arch)) {
                        struct JUNIT_Testsuite *testsuite = junitxml_testsuite_read(filename);
                        if (testsuite) {
                            if (globals.verbose) {
                                printf("%s: duration: %0.4f, failed: %d, skipped: %d, errors: %d\n", filename, testsuite->time, testsuite->failures, testsuite->skipped, testsuite->errors);
                            }
                            fprintf(indexfp, "|[%s](%s)|%0.4f|%d|%d|%d|\n", filename, filename, testsuite->time, testsuite->failures, testsuite->skipped, testsuite->errors);
                            /*
                             * TODO: Display failure/skip/error output.
                             */
                            char *bname = strdup(filename);
                            bname[strlen(bname) - 4] = 0;
                            char result_outfile[PATH_MAX] = {0};
                            path_basename(bname);
                            mkdir(bname, 0755);
                            snprintf(result_outfile, sizeof(result_outfile) - 1, "%s/%s.md", bname, bname);
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
                                    fprintf(resultfp, "### %s %s :: %s\n", type_str, testsuite->testcase[i]->classname, testsuite->testcase[i]->name);
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
                            fprintf(indexfp, "|No data|-|-|-|-|-|-|\n");
                            no_printable_data++;
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


