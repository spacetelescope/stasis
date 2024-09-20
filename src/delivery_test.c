#include "delivery.h"

void delivery_tests_run(struct Delivery *ctx) {
    struct MultiProcessingPool *pool_parallel;
    struct MultiProcessingPool *pool_serial;
    struct MultiProcessingPool *pool_setup;
    struct Process proc;
    memset(&proc, 0, sizeof(proc));

    if (!globals.workaround.conda_reactivate) {
        globals.workaround.conda_reactivate = calloc(PATH_MAX, sizeof(*globals.workaround.conda_reactivate));
    } else {
        memset(globals.workaround.conda_reactivate, 0, PATH_MAX);
    }
    snprintf(globals.workaround.conda_reactivate, PATH_MAX - 1, "\nmamba activate ${CONDA_DEFAULT_ENV}\n");

    if (!ctx->tests[0].name) {
        msg(STASIS_MSG_WARN | STASIS_MSG_L2, "no tests are defined!\n");
    } else {
        pool_parallel = mp_pool_init("parallel", ctx->storage.tmpdir);
        if (!pool_parallel) {
            perror("mp_pool_init/parallel");
            exit(1);
        }

        pool_serial = mp_pool_init("serial", ctx->storage.tmpdir);
        if (!pool_serial) {
            perror("mp_pool_init/serial");
            exit(1);
        }

        pool_setup = mp_pool_init("setup", ctx->storage.tmpdir);
        if (!pool_setup) {
            perror("mp_pool_init/setup");
            exit(1);
        }

        const char *runner_cmd_fmt = "set -e -x\n%s\n";
        for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
            struct Test *test = &ctx->tests[i];
            if (!test->name && !test->repository && !test->script) {
                // skip unused test records
                continue;
            }
            msg(STASIS_MSG_L2, "Executing tests for %s %s\n", test->name, test->version);
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

                msg(STASIS_MSG_L3, "Testing %s\n", test->name);
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

                char runner_cmd[0xFFFF] = {0};
                char pool_name[100] = "parallel";
                struct MultiProcessingTask *task = NULL;
                struct MultiProcessingPool *pool = pool_parallel;
                if (!test->parallel) {
                    pool = pool_serial;
                    memset(pool_name, 0, sizeof(pool_name));
                    strcpy(pool_name, "serial");
                }

                sprintf(runner_cmd, runner_cmd_fmt, cmd);
                task = mp_pool_task(pool, test->name, runner_cmd);
                if (!task) {
                    SYSERROR("Failed to add task to %s pool: %s", pool_name, runner_cmd);
                    popd();
                    if (!globals.continue_on_error) {
                        tpl_free();
                        delivery_free(ctx);
                        globals_free();
                    }
                    exit(1);
                }
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
                    char *cmd = calloc(strlen(test->script_setup) + STASIS_BUFSIZ, sizeof(*cmd));
                    if (!cmd) {
                        SYSERROR("Unable to allocate test script_setup buffer: %s", strerror(errno));
                        exit(1);
                    }

                    strcpy(cmd, test->script_setup);
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

                    struct MultiProcessingPool *pool = pool_setup;
                    struct MultiProcessingTask *task = NULL;
                    char runner_cmd[0xFFFF] = {0};
                    sprintf(runner_cmd, runner_cmd_fmt, cmd);

                    task = mp_pool_task(pool, test->name, runner_cmd);
                    if (!task) {
                        SYSERROR("Failed to add task %s to setup pool: %s", test->name, runner_cmd);
                        popd();
                        if (!globals.continue_on_error) {
                            tpl_free();
                            delivery_free(ctx);
                            globals_free();
                        }
                        exit(1);
                    }
                    guard_free(cmd);
                    popd();
                }
            }
        }

        size_t opt_flags = 0;
        if (globals.parallel_fail_fast) {
            opt_flags |= MP_POOL_FAIL_FAST;
        }

        int pool_status = -1;
        if (pool_setup->num_used) {
            pool_status = mp_pool_join(pool_setup, 1, opt_flags);
            mp_pool_show_summary(pool_setup);
            COE_CHECK_ABORT(pool_status != 0, "Failure in setup task pool");
            mp_pool_free(&pool_setup);
        }

        if (pool_parallel->num_used) {
            pool_status = mp_pool_join(pool_parallel, globals.cpu_limit, opt_flags);
            mp_pool_show_summary(pool_parallel);
            COE_CHECK_ABORT(pool_status != 0, "Failure in parallel task pool");
            mp_pool_free(&pool_parallel);
        }

        if (pool_serial->num_used) {
            pool_status = mp_pool_join(pool_serial, 1, opt_flags);
            mp_pool_show_summary(pool_serial);
            COE_CHECK_ABORT(pool_serial != 0, "Failure in serial task pool");
            mp_pool_free(&pool_serial);
        }
    }
}

int delivery_fixup_test_results(struct Delivery *ctx) {
    struct dirent *rec;
    DIR *dp;

    dp = opendir(ctx->storage.results_dir);
    if (!dp) {
        perror(ctx->storage.results_dir);
        return -1;
    }

    while ((rec = readdir(dp)) != NULL) {
        char path[PATH_MAX];
        memset(path, 0, sizeof(path));

        if (!strcmp(rec->d_name, ".") || !strcmp(rec->d_name, "..")) {
            continue;
        } else if (!endswith(rec->d_name, ".xml")) {
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

