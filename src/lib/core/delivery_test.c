#include "delivery.h"

void delivery_tests_run(struct Delivery *ctx) {
    static const int SETUP = 0;
    static const int PARALLEL = 1;
    static const int SERIAL = 2;
    struct MultiProcessingPool *pool[3];
    struct Process proc = {0};

    if (!globals.workaround.conda_reactivate) {
        globals.workaround.conda_reactivate = calloc(PATH_MAX, sizeof(*globals.workaround.conda_reactivate));
    } else {
        memset(globals.workaround.conda_reactivate, 0, PATH_MAX);
    }
    // Test blocks always run with xtrace enabled. Disable, and reenable it. Conda's wrappers produce an incredible
    // amount of debug information.
    snprintf(globals.workaround.conda_reactivate, PATH_MAX - 1, "\nset +x; mamba activate ${CONDA_DEFAULT_ENV}; set -x\n");

    if (!ctx->tests[0].name) {
        msg(STASIS_MSG_WARN | STASIS_MSG_L2, "no tests are defined!\n");
    } else {
        pool[PARALLEL] = mp_pool_init("parallel", ctx->storage.tmpdir);
        if (!pool[PARALLEL]) {
            perror("mp_pool_init/parallel");
            exit(1);
        }
        pool[PARALLEL]->status_interval = globals.pool_status_interval;

        pool[SERIAL] = mp_pool_init("serial", ctx->storage.tmpdir);
        if (!pool[SERIAL]) {
            perror("mp_pool_init/serial");
            exit(1);
        }
        pool[SERIAL]->status_interval = globals.pool_status_interval;

        pool[SETUP] = mp_pool_init("setup", ctx->storage.tmpdir);
        if (!pool[SETUP]) {
            perror("mp_pool_init/setup");
            exit(1);
        }
        pool[SETUP]->status_interval = globals.pool_status_interval;

        // Test block scripts shall exit non-zero on error.
        // This will fail a test block immediately if "string" is not found in file.txt:
        //      grep string file.txt
        //
        // And this is how to avoid that scenario:
        // #1:
        //      if ! grep string file.txt; then
        //          # handle error
        //      fi
        //
        //  #2:
        //      grep string file.txt || handle error
        //
        //  #3:
        //      # Use ':' as a NO-OP if/when the result doesn't matter
        //      grep string file.txt || :
        const char *runner_cmd_fmt = "set -e -x\n%s\n";

        // Iterate over our test records, retrieving the source code for each package, and assigning its scripted tasks
        // to the appropriate processing pool
        for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
            struct Test *test = &ctx->tests[i];
            if (!test->name && !test->repository && !test->script) {
                // skip unused test records
                continue;
            }
            msg(STASIS_MSG_L2, "Loading tests for %s %s\n", test->name, test->version);
            if (!test->script || !strlen(test->script)) {
                msg(STASIS_MSG_WARN | STASIS_MSG_L3, "Nothing to do. To fix, declare a 'script' in section: [test:%s]\n",
                    test->name);
                continue;
            }

            char destdir[PATH_MAX];
            sprintf(destdir, "%s/%s", ctx->storage.build_sources_dir, path_basename(test->repository));

            if (!access(destdir, F_OK)) {
                msg(STASIS_MSG_L3, "Purging repository %s\n", destdir);
                if (rmtree(destdir)) {
                    COE_CHECK_ABORT(1, "Unable to remove repository\n");
                }
            }
            msg(STASIS_MSG_L3, "Cloning repository %s\n", test->repository);
            if (!git_clone(&proc, test->repository, destdir, test->version)) {
                test->repository_info_tag = strdup(git_describe(destdir));
                test->repository_info_ref = strdup(git_rev_parse(destdir, "HEAD"));
            } else {
                COE_CHECK_ABORT(1, "Unable to clone repository\n");
            }

            if (test->repository_remove_tags && strlist_count(test->repository_remove_tags)) {
                filter_repo_tags(destdir, test->repository_remove_tags);
            }

            if (pushd(destdir)) {
                COE_CHECK_ABORT(1, "Unable to enter repository directory\n");
            } else {
                char *cmd = calloc(strlen(test->script) + STASIS_BUFSIZ, sizeof(*cmd));
                if (!cmd) {
                    SYSERROR("Unable to allocate test script buffer: %s", strerror(errno));
                    exit(1);
                }

                msg(STASIS_MSG_L3, "Queuing task for %s\n", test->name);
                memset(&proc, 0, sizeof(proc));

                strcpy(cmd, test->script);
                char *cmd_rendered = tpl_render(cmd);
                if (cmd_rendered) {
                    if (strcmp(cmd_rendered, cmd) != 0) {
                        strcpy(cmd, cmd_rendered);
                        cmd[strlen(cmd_rendered) ? strlen(cmd_rendered) - 1 : 0] = 0;
                    }
                    guard_free(cmd_rendered);
                } else {
                    SYSERROR("An error occurred while rendering the following:\n%s", cmd);
                    exit(1);
                }

                if (test->disable) {
                    msg(STASIS_MSG_L2, "Script execution disabled by configuration\n", test->name);
                    guard_free(cmd);
                    continue;
                }

                char *runner_cmd = NULL;
                char pool_name[100] = "parallel";
                struct MultiProcessingTask *task = NULL;
                int selected = PARALLEL;
                if (!globals.enable_parallel || !test->parallel) {
                    selected = SERIAL;
                    memset(pool_name, 0, sizeof(pool_name));
                    strcpy(pool_name, "serial");
                }

                if (asprintf(&runner_cmd, runner_cmd_fmt, cmd) < 0) {
                    SYSERROR("Unable to allocate memory for runner command: %s", strerror(errno));
                    exit(1);
                }
                task = mp_pool_task(pool[selected], test->name, destdir, runner_cmd);
                if (!task) {
                    SYSERROR("Failed to add task to %s pool: %s", pool_name, runner_cmd);
                    popd();
                    if (!globals.continue_on_error) {
                        guard_free(runner_cmd);
                        tpl_free();
                        delivery_free(ctx);
                        globals_free();
                    }
                    exit(1);
                }
                guard_free(runner_cmd);
                guard_free(cmd);
                popd();

            }
        }

        // Configure "script_setup" tasks
        // Directories should exist now, so no need to go through initializing everything all over again.
        for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
            struct Test *test = &ctx->tests[i];
            if (test->script_setup) {
                char destdir[PATH_MAX];
                sprintf(destdir, "%s/%s", ctx->storage.build_sources_dir, path_basename(test->repository));
                if (access(destdir, F_OK)) {
                    SYSERROR("%s: %s", destdir, strerror(errno));
                    exit(1);
                }
                if (!pushd(destdir)) {
                    const size_t cmd_len = strlen(test->script_setup) + STASIS_BUFSIZ;
                    char *cmd = calloc(cmd_len, sizeof(*cmd));
                    if (!cmd) {
                        SYSERROR("Unable to allocate test script_setup buffer: %s", strerror(errno));
                        exit(1);
                    }

                    strncpy(cmd, test->script_setup, cmd_len - 1);
                    char *cmd_rendered = tpl_render(cmd);
                    if (cmd_rendered) {
                        if (strcmp(cmd_rendered, cmd) != 0) {
                            strncpy(cmd, cmd_rendered, cmd_len - 1);
                            cmd[strlen(cmd_rendered) ? strlen(cmd_rendered) - 1 : 0] = 0;
                        }
                        guard_free(cmd_rendered);
                    } else {
                        SYSERROR("An error occurred while rendering the following:\n%s", cmd);
                        exit(1);
                    }

                    struct MultiProcessingTask *task = NULL;
                    char *runner_cmd = NULL;
                    if (asprintf(&runner_cmd, runner_cmd_fmt, cmd) < 0) {
                        SYSERROR("Unable to allocate memory for runner command: %s", strerror(errno));
                        exit(1);
                    }

                    task = mp_pool_task(pool[SETUP], test->name, destdir, runner_cmd);
                    if (!task) {
                        SYSERROR("Failed to add task %s to setup pool: %s", test->name, runner_cmd);
                        popd();
                        if (!globals.continue_on_error) {
                            guard_free(runner_cmd);
                            tpl_free();
                            delivery_free(ctx);
                            globals_free();
                        }
                        exit(1);
                    }
                    guard_free(runner_cmd);
                    guard_free(cmd);
                    popd();
                } else {
                    SYSERROR("Failed to change directory: %s\n", destdir);
                    exit(1);
                }
            }
        }

        size_t opt_flags = 0;
        if (globals.parallel_fail_fast) {
            opt_flags |= MP_POOL_FAIL_FAST;
        }

        // Execute all queued tasks
        for (size_t p = 0; p < sizeof(pool) / sizeof(*pool); p++) {
            long jobs = globals.cpu_limit;

            if (!pool[p]->num_used) {
                // Skip empty pool
                continue;
            }

            // Setup tasks run sequentially
            if (p == (size_t) SETUP || p == (size_t) SERIAL) {
                jobs = 1;
            }

            // Run tasks in the pool
            // 1. Setup (builds)
            // 2. Parallel (fast jobs)
            // 3. Serial (long jobs)
            int pool_status = mp_pool_join(pool[p], jobs, opt_flags);

            // On error show a summary of the current pool, and die
            if (pool_status != 0) {
                mp_pool_show_summary(pool[p]);
                COE_CHECK_ABORT(true, "Task failure");
            }
        }

        // All tasks were successful
        for (size_t p = 0; p < sizeof(pool) / sizeof(*pool); p++) {
            if (pool[p]->num_used) {
                // Only show pools that actually had jobs to run
                mp_pool_show_summary(pool[p]);
            }
            mp_pool_free(&pool[p]);
        }
    }
}

int delivery_fixup_test_results(struct Delivery *ctx) {
    struct dirent *rec;

    DIR *dp = opendir(ctx->storage.results_dir);
    if (!dp) {
        perror(ctx->storage.results_dir);
        return -1;
    }

    while ((rec = readdir(dp)) != NULL) {
        char path[PATH_MAX] = {0};

        if (!strcmp(rec->d_name, ".") || !strcmp(rec->d_name, "..") || !endswith(rec->d_name, ".xml")) {
            continue;
        }

        sprintf(path, "%s/%s", ctx->storage.results_dir, rec->d_name);
        msg(STASIS_MSG_L3, "%s\n", rec->d_name);
        if (xml_pretty_print_in_place(path, STASIS_XML_PRETTY_PRINT_PROG, STASIS_XML_PRETTY_PRINT_ARGS)) {
            msg(STASIS_MSG_L3 | STASIS_MSG_WARN, "Failed to rewrite file '%s'\n", rec->d_name);
        }
    }

    closedir(dp);
    return 0;
}

