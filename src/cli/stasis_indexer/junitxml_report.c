//
// Created by jhunk on 11/15/24.
//

#include "core.h"
#include "callbacks.h"
#include "junitxml.h"
#include "junitxml_report.h"

static int is_file_in_listing(struct StrList *list, const char *pattern) {
    for (size_t i = 0; i < strlist_count(list); i++) {
        char const *path = strlist_item(list, i);
        if (!fnmatch(pattern, path, 0)) {
            return 1;
        }
    }
    return 0;
}

static int write_report_output(struct Delivery *ctx, FILE *destfp, const char *xmlfilename) {
    struct JUNIT_Testsuite *testsuite = junitxml_testsuite_read(xmlfilename);
    if (testsuite) {
        if (globals.verbose) {
            printf("%s: duration: %0.4f, total: %d, passed: %d, failed: %d, skipped: %d, errors: %d\n", xmlfilename,
                   testsuite->time, testsuite->tests,
                   testsuite->passed, testsuite->failures,
                   testsuite->skipped, testsuite->errors);
        }

        char *bname_tmp = strdup(xmlfilename);
        char *bname = strdup(path_basename(bname_tmp));
        if (endswith(bname, ".xml")) {
            bname[strlen(bname) - 4] = 0;
        }
        guard_free(bname_tmp);

        char result_outfile[PATH_MAX] = {0};
        char *short_name_pattern = NULL;
        asprintf(&short_name_pattern, "-%s", ctx->info.release_name);

        char short_name[PATH_MAX] = {0};
        strncpy(short_name, bname, sizeof(short_name) - 1);
        replace_text(short_name, short_name_pattern, "", 0);
        replace_text(short_name, "results-", "", 0);
        guard_free(short_name_pattern);

        fprintf(destfp, "|[%s](%s.html)|%0.4f|%d|%d|%d|%d|%d|\n", short_name, bname,
                testsuite->time, testsuite->tests,
                testsuite->passed, testsuite->failures,
                testsuite->skipped, testsuite->errors);

        snprintf(result_outfile, sizeof(result_outfile) - strlen(bname) - 3, "%s.md",
                 bname);
        guard_free(bname);

        FILE *resultfp = fopen(result_outfile, "w+");
        if (!resultfp) {
            SYSERROR("Unable to open %s for writing", result_outfile);
            return -1;
        }

        for (size_t i = 0; i < testsuite->_tc_inuse; i++) {
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
            } else {
                message = testsuite->testcase[i]->message ? testsuite->testcase[i]->message : "";
                type_str = "[PASSED]";
            }
            fprintf(resultfp, "### %s %s :: %s\n", type_str,
                    testsuite->testcase[i]->classname, testsuite->testcase[i]->name);
            fprintf(resultfp, "\nDuration: %0.04fs\n", testsuite->testcase[i]->time);
            fprintf(resultfp, "\n```\n%s\n```\n\n", message);
        }
        junitxml_testsuite_free(&testsuite);
        fclose(resultfp);
    } else {
        fprintf(stderr, "bad test suite: %s: %s\n", strerror(errno), xmlfilename);
    }
    return 0;
}

int indexer_junitxml_report(struct Delivery ctx[], const size_t nelem) {
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
        printf("index %s opened for writing", indexfile);

        for (size_t d = 0; d < nelem; d++) {
            char pattern[PATH_MAX] = {0};
            snprintf(pattern, sizeof(pattern) - 1, "*%s*", ctx[d].info.release_name);

            // if result directory contains this release name, print it
            fprintf(indexfp, "### %s\n", ctx[d].info.release_name);
            if (!is_file_in_listing(file_listing, pattern)) {
                fprintf(indexfp, "No test results\n");
                continue;
            }
            fprintf(indexfp, "\n|Suite|Duration|Total|Pass|Fail|Skip|Error|\n");
            fprintf(indexfp, "|:----|:------:|:---:|:--:|:--:|:--:|:---:|\n");

            for (size_t i = 0; i < strlist_count(file_listing); i++) {
                const char *filename = strlist_item(file_listing, i);
                // if not a xml file, skip it
                if (!endswith(filename, ".xml")) {
                    continue;
                }
                if (!fnmatch(pattern, filename, 0)) {
                    if (write_report_output(&ctx[d], indexfp, filename)) {
                        // warn only
                        SYSERROR("Unable to write xml report file using %s", filename);
                    }
                }
            }
            fprintf(indexfp, "\n");
        }
        fclose(indexfp);
        popd();
    } else {
        fprintf(stderr, "Unable to enter delivery directory: %s\n", ctx->storage.delivery_dir);
        guard_strlist_free(&file_listing);
        return -1;
    }

    guard_strlist_free(&file_listing);
    return 0;
}
