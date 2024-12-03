#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "core.h"
#include "delivery.h"

// local includes
#include "args.h"
#include "system_requirements.h"
#include "tpl.h"


int main(int argc, char *argv[]) {
    struct Delivery ctx;
    struct Process proc = {
            .f_stdout = "",
            .f_stderr = "",
            .redirect_stderr = 0,
    };
    char env_name[STASIS_NAME_MAX] = {0};
    char env_name_testing[STASIS_NAME_MAX] = {0};
    char *delivery_input = NULL;
    char *config_input = NULL;
    char installer_url[PATH_MAX];
    char python_override_version[STASIS_NAME_MAX];
    int user_disabled_docker = false;
    globals.cpu_limit = get_cpu_count();
    if (globals.cpu_limit > 1) {
        globals.cpu_limit--; // max - 1
    }

    memset(env_name, 0, sizeof(env_name));
    memset(env_name_testing, 0, sizeof(env_name_testing));
    memset(installer_url, 0, sizeof(installer_url));
    memset(python_override_version, 0, sizeof(python_override_version));
    memset(&proc, 0, sizeof(proc));
    memset(&ctx, 0, sizeof(ctx));

    int c;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "hVCc:p:vU", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                usage(path_basename(argv[0]));
                exit(0);
            case 'V':
                puts(VERSION);
                exit(0);
            case 'c':
                config_input = strdup(optarg);
                break;
            case 'C':
                globals.continue_on_error = true;
                break;
            case 'p':
                strcpy(python_override_version, optarg);
                break;
            case 'l':
                globals.cpu_limit = strtol(optarg, NULL, 10);
                if (globals.cpu_limit <= 1) {
                    globals.cpu_limit = 1;
                    globals.enable_parallel = false; // No point
                }
                break;
            case OPT_ALWAYS_UPDATE_BASE:
                globals.always_update_base_environment = true;
                break;
            case OPT_FAIL_FAST:
                globals.parallel_fail_fast = true;
                break;
            case OPT_POOL_STATUS_INTERVAL:
                globals.pool_status_interval = (int) strtol(optarg, NULL, 10);
                if (globals.pool_status_interval < 1) {
                    globals.pool_status_interval = 1;
                } else if (globals.pool_status_interval > 60 * 10) {
                    // Possible poor choice alert
                    fprintf(stderr, "Caution: Excessive pausing between status updates may cause third-party CI/CD"
                                    " jobs to fail if the stdout/stderr streams are idle for too long!\n");
                }
                break;
            case 'U':
                setenv("PYTHONUNBUFFERED", "1", 1);
                fflush(stdout);
                fflush(stderr);
                setvbuf(stdout, NULL, _IONBF, 0);
                setvbuf(stderr, NULL, _IONBF, 0);
                break;
            case 'v':
                globals.verbose = true;
                break;
            case OPT_OVERWRITE:
                globals.enable_overwrite = true;
                break;
            case OPT_NO_DOCKER:
                globals.enable_docker = false;
                user_disabled_docker = true;
                break;
            case OPT_NO_ARTIFACTORY:
                globals.enable_artifactory = false;
                break;
            case OPT_NO_ARTIFACTORY_BUILD_INFO:
                globals.enable_artifactory_build_info = false;
                break;
            case OPT_NO_TESTING:
                globals.enable_testing = false;
                break;
            case OPT_NO_REWRITE_SPEC_STAGE_2:
                globals.enable_rewrite_spec_stage_2 = false;
                break;
            case OPT_NO_PARALLEL:
                globals.enable_parallel = false;
                break;
            case '?':
            default:
                exit(1);
        }
    }

    if (optind < argc) {
        while (optind < argc) {
            // use first positional argument
            delivery_input = argv[optind++];
            break;
        }
    }

    if (!delivery_input) {
        fprintf(stderr, "error: a DELIVERY_FILE is required\n");
        usage(path_basename(argv[0]));
        exit(1);
    }

    printf(BANNER, VERSION, AUTHOR);

    check_system_path();

    msg(STASIS_MSG_L1, "Setup\n");

    tpl_setup_vars(&ctx);
    tpl_setup_funcs(&ctx);

    // Set up PREFIX/etc directory information
    // The user may manipulate the base directory path with STASIS_SYSCONFDIR
    // environment variable
    char stasis_sysconfdir_tmp[PATH_MAX];
    if (getenv("STASIS_SYSCONFDIR")) {
        strncpy(stasis_sysconfdir_tmp, getenv("STASIS_SYSCONFDIR"), sizeof(stasis_sysconfdir_tmp) - 1);
    } else {
        strncpy(stasis_sysconfdir_tmp, STASIS_SYSCONFDIR, sizeof(stasis_sysconfdir_tmp) - 1);
    }

    globals.sysconfdir = realpath(stasis_sysconfdir_tmp, NULL);
    if (!globals.sysconfdir) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "Unable to resolve path to configuration directory: %s\n", stasis_sysconfdir_tmp);
        exit(1);
    }

    // Override Python version from command-line, if any
    if (strlen(python_override_version)) {
        guard_free(ctx.meta.python);
        ctx.meta.python = strdup(python_override_version);
        guard_free(ctx.meta.python_compact);
        ctx.meta.python_compact = to_short_version(ctx.meta.python);
    }

    if (!config_input) {
        // no configuration passed by argument. use basic config.
        char cfgfile[PATH_MAX * 2];
        sprintf(cfgfile, "%s/%s", globals.sysconfdir, "stasis.ini");
        if (!access(cfgfile, F_OK | R_OK)) {
            config_input = strdup(cfgfile);
        } else {
            msg(STASIS_MSG_WARN, "STASIS global configuration is not readable, or does not exist: %s", cfgfile);
        }
    }

    if (config_input) {
        msg(STASIS_MSG_L2, "Reading STASIS global configuration: %s\n", config_input);
        ctx._stasis_ini_fp.cfg = ini_open(config_input);
        if (!ctx._stasis_ini_fp.cfg) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Failed to read config file: %s, %s\n", delivery_input, strerror(errno));
            exit(1);
        }
        ctx._stasis_ini_fp.cfg_path = strdup(config_input);
        guard_free(config_input);
    }

    msg(STASIS_MSG_L2, "Reading STASIS delivery configuration: %s\n", delivery_input);
    ctx._stasis_ini_fp.delivery = ini_open(delivery_input);
    if (!ctx._stasis_ini_fp.delivery) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Failed to read delivery file: %s, %s\n", delivery_input, strerror(errno));
        exit(1);
    }
    ctx._stasis_ini_fp.delivery_path = strdup(delivery_input);

    msg(STASIS_MSG_L2, "Bootstrapping delivery context\n");
    if (bootstrap_build_info(&ctx)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Failed to bootstrap delivery context\n");
        exit(1);
    }

    msg(STASIS_MSG_L2, "Initializing delivery context\n");
    if (delivery_init(&ctx, INI_READ_RENDER)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Failed to initialize delivery context\n");
        exit(1);
    }
    check_requirements(&ctx);

    msg(STASIS_MSG_L2, "Configuring JFrog CLI\n");
    if (delivery_init_artifactory(&ctx)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "JFrog CLI configuration failed\n");
        exit(1);
    }

    runtime_apply(ctx.runtime.environ);
    strcpy(env_name, ctx.info.release_name);
    strcpy(env_name_testing, env_name);
    strcat(env_name_testing, "-test");

    // Safety gate: Avoid clobbering a delivered release unless the user wants that behavior
    msg(STASIS_MSG_L1, "Checking release history\n");
    if (!globals.enable_overwrite && delivery_exists(&ctx) == DELIVERY_FOUND) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "Refusing to overwrite release: %s\nUse --overwrite to enable release clobbering.\n", ctx.info.release_name);
        exit(1);
    }

    if (globals.enable_artifactory) {
        // We need to download previous revisions to ensure processed packages are available at build-time
        // This is also a docker requirement. Python wheels must exist locally.
        if (ctx.meta.rc > 1) {
            msg(STASIS_MSG_L1, "Syncing delivery artifacts for %s\n", ctx.info.build_name);
            if (delivery_series_sync(&ctx) != 0) {
                msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Unable to sync artifacts for %s\n", ctx.info.build_name);
                msg(STASIS_MSG_L3, "Case #1:\n"
                                   "\tIf this is a new 'version', and 'rc' is greater "
                                   "than 1, then no previous deliveries exist remotely. "
                                   "Reset 'rc' to 1.\n");
                msg(STASIS_MSG_L3, "Case #2:\n"
                                   "\tThe Artifactory server %s is unreachable, or the credentials used "
                                   "are invalid.\n", globals.jfrog.url);
                // No continue-on-error check. Without the previous delivery nothing can be done.
                exit(1);
            }
        }
    }

    // Unlikely to occur: this should help prevent rmtree() from destroying your entire filesystem
    // if path is "/" then, die
    // or if empty string, die
    if (!strcmp(ctx.storage.conda_install_prefix, DIR_SEP) || !strlen(ctx.storage.conda_install_prefix)) {
        fprintf(stderr, "error: ctx.storage.conda_install_prefix is malformed!\n");
        exit(1);
    }

    // 2 = #!
    // 5 = /bin\n
    const size_t prefix_len = strlen(ctx.storage.conda_install_prefix) + 2 + 5;
    const size_t prefix_len_max = 127;
    msg(STASIS_MSG_L1, "Checking length of conda installation prefix\n");
    if (!strcmp(ctx.system.platform[DELIVERY_PLATFORM], "Linux") && prefix_len > prefix_len_max) {
        msg(STASIS_MSG_L2 | STASIS_MSG_ERROR,
            "The shebang, '#!%s/bin/python\\n' is too long (%zu > %zu).\n",
            ctx.storage.conda_install_prefix, prefix_len, prefix_len_max);
        msg(STASIS_MSG_L2 | STASIS_MSG_ERROR,
            "Conda's workaround to handle long path names does not work consistently within STASIS.\n");
        msg(STASIS_MSG_L2 | STASIS_MSG_ERROR,
            "Please try again from a different, \"shorter\", directory.\n");
        exit(1);
    }

    msg(STASIS_MSG_L1, "Conda setup\n");
    delivery_get_conda_installer_url(&ctx, installer_url);
    msg(STASIS_MSG_L2, "Downloading: %s\n", installer_url);
    if (delivery_get_conda_installer(&ctx, installer_url)) {
        msg(STASIS_MSG_ERROR, "download failed: %s\n", installer_url);
        exit(1);
    }

    msg(STASIS_MSG_L2, "Installing: %s\n", ctx.conda.installer_name);
    delivery_install_conda(ctx.conda.installer_path, ctx.storage.conda_install_prefix);

    msg(STASIS_MSG_L2, "Configuring: %s\n", ctx.storage.conda_install_prefix);
    delivery_conda_enable(&ctx, ctx.storage.conda_install_prefix);

    //
    // Implied environment creation modes/actions
    //
    // 1. No base environment config
    //   1a. Caller is warned
    //   1b. Caller has full control over all packages
    // 2. Default base environment (etc/stasis/mission/[name]/base.yml)
    //   2a. Depends on packages defined by base.yml
    //   2b. Caller may issue a reduced package set in the INI config
    //   2c. Caller must be vigilant to avoid incompatible packages (base.yml
    //       *should* have no version constraints)
    // 3. External base environment (based_on=schema://[release_name].yml)
    //   3a. Depends on a previous release or arbitrary yaml configuration
    //   3b. Bugs, conflicts, and dependency resolution issues are inherited and
    //       must be handled in the INI config
    msg(STASIS_MSG_L1, "Creating release environment(s)\n");

    char *mission_base = NULL;
    if (isempty(ctx.meta.based_on)) {
        guard_free(ctx.meta.based_on);
        char *mission_base_orig = NULL;

        if (asprintf(&mission_base_orig, "%s/%s/base.yml", ctx.storage.mission_dir, ctx.meta.mission) < 0) {
            SYSERROR("Unable to allocate bytes for %s/%s/base.yml path\n", ctx.storage.mission_dir, ctx.meta.mission);
            exit(1);
        }

        if (access(mission_base_orig, F_OK) < 0) {
            msg(STASIS_MSG_L2 | STASIS_MSG_WARN, "Mission does not provide a base.yml configuration: %s (%s)\n",
                ctx.meta.mission, ctx.storage.mission_dir);
        } else {
            msg(STASIS_MSG_L2, "Using base environment configuration: %s\n", mission_base_orig);
            if (asprintf(&mission_base, "%s/%s-base.yml", ctx.storage.tmpdir, ctx.info.release_name) < 0) {
                SYSERROR("%s", "Unable to allocate bytes for temporary base.yml configuration");
                remove(mission_base);
                exit(1);
            }
            copy2(mission_base_orig, mission_base, CT_OWNER | CT_PERM);
            char spec[255] = {0};
            snprintf(spec, sizeof(spec) - 1, "- python=%s\n", ctx.meta.python);
            file_replace_text(mission_base, "- python\n", spec, 0);
            ctx.meta.based_on = mission_base;
        }
        guard_free(mission_base_orig);
    }

    if (!isempty(ctx.meta.based_on)) {
        if (conda_env_exists(ctx.storage.conda_install_prefix, env_name) && conda_env_remove(env_name)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed to remove release environment: %s\n", env_name);
            exit(1);
        }

        msg(STASIS_MSG_L2, "Based on: %s\n", ctx.meta.based_on);
        if (conda_env_create_from_uri(env_name, ctx.meta.based_on)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "unable to install release environment using configuration file\n");
            exit(1);
        }

        if (conda_env_exists(ctx.storage.conda_install_prefix, env_name_testing) && conda_env_remove(env_name_testing)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed to remove testing environment %s\n", env_name_testing);
            exit(1);
        }
        if (conda_env_create_from_uri(env_name_testing, ctx.meta.based_on)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "unable to install testing environment using configuration file\n");
            exit(1);
        }
    } else {
        if (conda_env_create(env_name, ctx.meta.python, NULL)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed to create release environment\n");
            exit(1);
        }
        if (conda_env_create(env_name_testing, ctx.meta.python, NULL)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed to create testing environment\n");
            exit(1);
        }
    }
    // The base environment configuration not used past this point
    remove(mission_base);

    // Activate test environment
    msg(STASIS_MSG_L1, "Activating test environment\n");
    if (conda_activate(ctx.storage.conda_install_prefix, env_name_testing)) {
        fprintf(stderr, "failed to activate test environment\n");
        exit(1);
    }

    if (delivery_gather_tool_versions(&ctx)) {
        if (!ctx.conda.tool_version) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Could not determine conda version\n");
            exit(1);
        }
        if (!ctx.conda.tool_build_version) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Could not determine conda-build version\n");
            exit(1);
        }
    }

    if (pip_exec("install build")) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "'build' tool installation failed\n");
        exit(1);
    }

    if (!isempty(ctx.meta.based_on)) {
        msg(STASIS_MSG_L1, "Generating package overlay from environment: %s\n", env_name);
        if (delivery_overlay_packages_from_env(&ctx, env_name)) {
            msg(STASIS_MSG_L2 | STASIS_MSG_ERROR, "%s", "Failed to generate package overlay. Resulting environment integrity cannot be guaranteed.\n");
            exit(1);
        }
    }

    msg(STASIS_MSG_L1, "Filter deliverable packages\n");
    delivery_defer_packages(&ctx, DEFER_CONDA);
    delivery_defer_packages(&ctx, DEFER_PIP);

    msg(STASIS_MSG_L1, "Overview\n");
    delivery_meta_show(&ctx);
    delivery_conda_show(&ctx);
    if (globals.verbose) {
        //delivery_runtime_show(&ctx);
    }

    // Execute configuration-defined tests
    if (globals.enable_testing) {
        delivery_tests_show(&ctx);

        msg(STASIS_MSG_L1, "Begin test execution\n");
        delivery_tests_run(&ctx);
        msg(STASIS_MSG_L2, "Rewriting test results\n");
        delivery_fixup_test_results(&ctx);
    } else {
        msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Test execution is disabled\n");
    }

    if (ctx.conda.conda_packages_defer && strlist_count(ctx.conda.conda_packages_defer)) {
        msg(STASIS_MSG_L2, "Building Conda recipe(s)\n");
        if (delivery_build_recipes(&ctx)) {
            exit(1);
        }
        msg(STASIS_MSG_L3, "Copying artifacts\n");
        if (delivery_copy_conda_artifacts(&ctx)) {
            exit(1);
        }
        msg(STASIS_MSG_L3, "Indexing artifacts\n");
        if (delivery_index_conda_artifacts(&ctx)) {
            exit(1);
        }
    }

    if (strlist_count(ctx.conda.pip_packages_defer)) {
        if (!((ctx.conda.wheels_packages = delivery_build_wheels(&ctx)))) {
            exit(1);
        }
        if (delivery_index_wheel_artifacts(&ctx)) {
            exit(1);
        }

    }

    // Populate the release environment
    msg(STASIS_MSG_L1, "Populating release environment\n");
    msg(STASIS_MSG_L2, "Installing conda packages\n");
    if (strlist_count(ctx.conda.conda_packages)) {
        if (delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_CONDA, (struct StrList *[]) {ctx.conda.conda_packages, NULL})) {
            exit(1);
        }
    }
    if (strlist_count(ctx.conda.conda_packages_defer)) {
        msg(STASIS_MSG_L3, "Installing deferred conda packages\n");
        if (delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_CONDA | INSTALL_PKG_CONDA_DEFERRED, (struct StrList *[]) {ctx.conda.conda_packages_defer, NULL})) {
            exit(1);
        }
    } else {
        msg(STASIS_MSG_L3, "No deferred conda packages\n");
    }

    msg(STASIS_MSG_L2, "Installing pip packages\n");
    if (strlist_count(ctx.conda.pip_packages)) {
        if (delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_PIP, (struct StrList *[]) {ctx.conda.pip_packages, NULL})) {
            exit(1);
        }
    }

    if (strlist_count(ctx.conda.pip_packages_defer)) {
        msg(STASIS_MSG_L3, "Installing deferred pip packages\n");
        if (delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_PIP | INSTALL_PKG_PIP_DEFERRED, (struct StrList *[]) {ctx.conda.pip_packages_defer, NULL})) {
            exit(1);
        }
    } else {
        msg(STASIS_MSG_L3, "No deferred pip packages\n");
    }

    conda_exec("list");

    msg(STASIS_MSG_L1, "Creating release\n");
    msg(STASIS_MSG_L2, "Exporting delivery configuration\n");
    if (!pushd(ctx.storage.cfgdump_dir)) {
        char filename[PATH_MAX] = {0};
        sprintf(filename, "%s.ini", ctx.info.release_name);
        FILE *spec = fopen(filename, "w+");
        if (!spec) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed %s\n", filename);
            exit(1);
        }
        ini_write(ctx._stasis_ini_fp.delivery, &spec, INI_WRITE_RAW);
        fclose(spec);

        memset(filename, 0, sizeof(filename));
        sprintf(filename, "%s-rendered.ini", ctx.info.release_name);
        spec = fopen(filename, "w+");
        if (!spec) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed %s\n", filename);
            exit(1);
        }
        ini_write(ctx._stasis_ini_fp.delivery, &spec, INI_WRITE_PRESERVE);
        fclose(spec);
        popd();
    } else {
        SYSERROR("Failed to enter directory: %s", ctx.storage.delivery_dir);
        exit(1);
    }

    msg(STASIS_MSG_L2, "Exporting %s\n", env_name_testing);
    if (conda_env_export(env_name_testing, ctx.storage.delivery_dir, env_name_testing)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed %s\n", env_name_testing);
        exit(1);
    }

    msg(STASIS_MSG_L2, "Exporting %s\n", env_name);
    if (conda_env_export(env_name, ctx.storage.delivery_dir, env_name)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed %s\n", env_name);
        exit(1);
    }

    // Rewrite release environment output (i.e. set package origin(s) to point to the deployment server, etc.)
    char specfile[PATH_MAX];
    sprintf(specfile, "%s/%s.yml", ctx.storage.delivery_dir, env_name);
    msg(STASIS_MSG_L3, "Rewriting release spec file (stage 1): %s\n", path_basename(specfile));
    delivery_rewrite_spec(&ctx, specfile, DELIVERY_REWRITE_SPEC_STAGE_1);

    msg(STASIS_MSG_L1, "Rendering mission templates\n");
    delivery_mission_render_files(&ctx);

    int want_docker = ini_section_search(&ctx._stasis_ini_fp.delivery, INI_SEARCH_BEGINS, "deploy:docker") ? true : false;
    int want_artifactory = ini_section_search(&ctx._stasis_ini_fp.delivery, INI_SEARCH_BEGINS, "deploy:artifactory") ? true : false;

    if (want_docker) {
        if (user_disabled_docker) {
            msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Docker image building is disabled by CLI argument\n");
        } else {
            char dockerfile[PATH_MAX] = {0};
            sprintf(dockerfile, "%s/%s", ctx.storage.build_docker_dir, "Dockerfile");
            if (globals.enable_docker) {
                if (!access(dockerfile, F_OK)) {
                    msg(STASIS_MSG_L1, "Building Docker image\n");
                    if (delivery_docker(&ctx)) {
                        msg(STASIS_MSG_L1 | STASIS_MSG_ERROR, "Failed to build docker image!\n");
                        COE_CHECK_ABORT(1, "Failed to build docker image");
                    }
                } else {
                    msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Docker image building is disabled. No Dockerfile found in %s\n", ctx.storage.build_docker_dir);
                }
            } else {
                msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Docker image building is disabled. System configuration error\n");
            }
        }
    } else {
        msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Docker image building is disabled. deploy:docker is not configured\n");
    }

    msg(STASIS_MSG_L3, "Rewriting release spec file (stage 2): %s\n", path_basename(specfile));
    delivery_rewrite_spec(&ctx, specfile, DELIVERY_REWRITE_SPEC_STAGE_2);

    msg(STASIS_MSG_L1, "Dumping metadata\n");
    if (delivery_dump_metadata(&ctx)) {
        msg(STASIS_MSG_L1 | STASIS_MSG_ERROR, "Metadata dump failed\n");
    }

    if (want_artifactory) {
        if (globals.enable_artifactory) {
            msg(STASIS_MSG_L1, "Uploading artifacts\n");
            delivery_artifact_upload(&ctx);
        } else {
            msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Artifactory upload is disabled by CLI argument\n");
        }
    } else {
        msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Artifactory upload is disabled. deploy:artifactory is not configured\n");
    }

    msg(STASIS_MSG_L1, "Cleaning up\n");
    delivery_free(&ctx);
    globals_free();
    tpl_free();

    msg(STASIS_MSG_L1, "Done!\n");
    return 0;
}

