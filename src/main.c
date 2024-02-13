#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include "omc.h"

const char *VERSION = "1.0.0";
const char *AUTHOR = "Joseph Hunkeler";
const char *BANNER = "---------------------------------------------------------------------\n"
                     " ██████╗ ██╗  ██╗    ███╗   ███╗██╗   ██╗     ██████╗ █████╗ ██╗     \n"
                     "██╔═══██╗██║  ██║    ████╗ ████║╚██╗ ██╔╝    ██╔════╝██╔══██╗██║     \n"
                     "██║   ██║███████║    ██╔████╔██║ ╚████╔╝     ██║     ███████║██║     \n"
                     "██║   ██║██╔══██║    ██║╚██╔╝██║  ╚██╔╝      ██║     ██╔══██║██║     \n"
                     "╚██████╔╝██║  ██║    ██║ ╚═╝ ██║   ██║       ╚██████╗██║  ██║███████╗\n"
                     " ╚═════╝ ╚═╝  ╚═╝    ╚═╝     ╚═╝   ╚═╝        ╚═════╝╚═╝  ╚═╝╚══════╝\n"
                     "---------------------------------------------------------------------\n"
                     "                          Delivery Generator                         \n"
                     "                                v%s                                  \n"
                     "---------------------------------------------------------------------\n"
                     "Copyright (C) 2023-2024 %s,\n"
                     "Association of Universities for Research in Astronomy (AURA)\n";

struct OMC_GLOBAL globals = {
    .verbose = false,
    .continue_on_error = false,
    .always_update_base_environment = false,
    .conda_fresh_start = true,
    .conda_install_prefix = NULL,
    .conda_packages = NULL,
    .pip_packages = NULL,
    .tmpdir = NULL,
};

#define OPT_ALWAYS_UPDATE_BASE 1000
static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {"continue-on-error", no_argument, 0, 'C'},
        {"config", optional_argument, 0, 'c'},
        {"python", optional_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'v'},
        {"update-base", optional_argument, 0, OPT_ALWAYS_UPDATE_BASE},
        {0, 0, 0, 0},
};

const char *long_options_help[] = {
        "Display this usage statement",
        "Display program version",
        "Allow tests to fail",
        "Read configuration file",
        "Override version of Python in configuration",
        "Increase output verbosity",
        "Update conda installation prior to OMC environment creation",
        NULL,
};

static int get_option_max_width(struct option option[]) {
    int i = 0;
    int max = 0;
    while (option[i].name != 0) {
        int len = (int) strlen(option[i].name);
        if (option[i].has_arg) {
            len += 4;
        }
        if (len > max) {
            max = len;
        }
        i++;
    }
    return max;
}

static void usage(char *progname) {
    printf("usage: %s ", progname);
    printf("[-");
    for (int x = 0; long_options[x].val != 0; x++) {
        if (long_options[x].has_arg == no_argument) {
            putchar(long_options[x].val);
        }
    }
    printf("] {DELIVERY_FILE}\n");

    int width = get_option_max_width(long_options);
    for (int x = 0; long_options[x].name != 0; x++) {
        char tmp[OMC_NAME_MAX] = {0};
        char output[sizeof(tmp)] = {0};
        char opt_long[50] = {0};        // --? [ARG]?
        char opt_short[3] = {0};        // -?

        strcat(opt_long, "--");
        strcat(opt_long, long_options[x].name);
        if (long_options[x].has_arg) {
            strcat(opt_long, " ARG");
        }

        if (long_options[x].val <= 'z') {
            strcat(opt_short, "-");
            opt_short[1] = (char) long_options[x].val;
        } else {
            strcat(opt_short, "  ");
        }

        sprintf(tmp, "  %%-%ds\t%%s\t\t%%s", width + 4);
        sprintf(output, tmp, opt_long, opt_short, long_options_help[x]);
        puts(output);
    }
}

void globals_free() {
    guard_free(globals.tmpdir)
    guard_free(globals.sysconfdir)
    guard_free(globals.conda_install_prefix)
    guard_strlist_free(globals.conda_packages)
    guard_strlist_free(globals.pip_packages)
}

int main(int argc, char *argv[], char *arge[]) {
    struct INIFILE *cfg = NULL;
    struct INIFILE *ini = NULL;
    struct Delivery ctx;
    struct Process proc = {
            .f_stdout = "",
            .f_stderr = "",
            .redirect_stderr = 0,
    };
    char env_name[OMC_NAME_MAX] = {0};
    char env_name_testing[OMC_NAME_MAX] = {0};
    char *delivery_input = NULL;
    char *config_input = NULL;
    char installer_url[PATH_MAX];
    char python_override_version[OMC_NAME_MAX];
    unsigned char arg_continue_on_error = 0;
    unsigned char arg_always_update_base_environment = 0;

    int c;
    while (1) {
        int option_index;
        c = getopt_long(argc, argv, "hVCc:p:v", long_options, &option_index);
        if (c == -1) {
            break;
        }
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
                arg_continue_on_error = 1;
                break;
            case 'p':
                strcpy(python_override_version, optarg);
                break;
            case OPT_ALWAYS_UPDATE_BASE:
                arg_always_update_base_environment = 1;
                break;
            case 'v':
                globals.verbose = 1;
                break;
            case '?':
                break;
            default:
                printf("unknown option: %c\n", c);
        }
    }

    if (optind < argc) {
        while (optind < argc) {
            // use first positional argument
            delivery_input = argv[optind];
            break;
        }
    }

    memset(&proc, 0, sizeof(proc));
    memset(&ctx, 0, sizeof(ctx));

    if (!delivery_input) {
        fprintf(stderr, "error: a DELIVERY_FILE is required\n");
        usage(path_basename(argv[0]));
        exit(1);
    }

    printf(BANNER, VERSION, AUTHOR);

    msg(OMC_MSG_L1, "Initializing\n");

    // Expose variables for use with the template engine
    // NOTE: These pointers are populated by delivery_init() so please avoid using
    // tpl_render() until then.
    tpl_register("meta.name", &ctx.meta.name);
    tpl_register("meta.version", &ctx.meta.version);
    tpl_register("meta.codename", &ctx.meta.codename);
    tpl_register("meta.mission", &ctx.meta.mission);
    tpl_register("meta.python", &ctx.meta.python);
    tpl_register("meta.python_compact", &ctx.meta.python_compact);
    tpl_register("info.time_str_epoch", &ctx.info.time_str_epoch);
    tpl_register("info.release_name", &ctx.info.release_name);
    tpl_register("conda.installer_baseurl", &ctx.conda.installer_baseurl);
    tpl_register("conda.installer_name", &ctx.conda.installer_name);
    tpl_register("conda.installer_version", &ctx.conda.installer_version);
    tpl_register("conda.installer_arch", &ctx.conda.installer_arch);
    tpl_register("conda.installer_platform", &ctx.conda.installer_platform);
    tpl_register("system.arch", &ctx.system.arch);
    tpl_register("system.platform", &ctx.system.platform[DELIVERY_PLATFORM_RELEASE]);

    // Set up PREFIX/etc directory information
    // The user may manipulate the base directory path with OMC_SYSCONFDIR
    // environment variable
    char omc_sysconfdir_tmp[PATH_MAX];
    if (getenv("OMC_SYSCONFDIR")) {
        strncpy(omc_sysconfdir_tmp, getenv("OMC_SYSCONFDIR"), sizeof(omc_sysconfdir_tmp));
    } else {
        strncpy(omc_sysconfdir_tmp, OMC_SYSCONFDIR, sizeof(omc_sysconfdir_tmp) - 1);
    }

    globals.sysconfdir = realpath(omc_sysconfdir_tmp, NULL);
    if (!globals.sysconfdir) {
        msg(OMC_MSG_ERROR | OMC_MSG_L1, "Unable to resolve path to configuration directory: %s\n", omc_sysconfdir_tmp);
        exit(1);
    } else {
        memset(omc_sysconfdir_tmp, 0, sizeof(omc_sysconfdir_tmp));
    }

    if (config_input) {
        msg(OMC_MSG_L2, "Reading OMC global configuration: %s\n", config_input);
        cfg = ini_open(config_input);
        if (!cfg) {
            msg(OMC_MSG_ERROR | OMC_MSG_L2, "Failed to read config file: %s, %s\n", delivery_input, strerror(errno));
            exit(1);
        }
    }

    msg(OMC_MSG_L2, "Reading OMC delivery configuration: %s\n", delivery_input);
    ini = ini_open(delivery_input);
    if (!ini) {
        msg(OMC_MSG_ERROR | OMC_MSG_L2, "Failed to read delivery file: %s, %s\n", delivery_input, strerror(errno));
        exit(1);
    }

    if (delivery_init(&ctx, ini, cfg)) {
        msg(OMC_MSG_ERROR | OMC_MSG_L1, "Failed to initialize delivery context\n");
        exit(1);
    }

    globals.always_update_base_environment = arg_always_update_base_environment;
    globals.continue_on_error = arg_continue_on_error;

    // Override Python version from command-line, if any
    // TODO: Investigate better method to override.
    //       And make sure all pointers/strings are updated
    if (strlen(python_override_version) && ctx.meta.python) {
        guard_free(ctx.meta.python)
        ctx.meta.python = strdup(python_override_version);
    }

    msg(OMC_MSG_L2, "Configuring JFrog CLI\n");
    if (delivery_init_artifactory(&ctx)) {
        msg(OMC_MSG_ERROR | OMC_MSG_L2, "JFrog CLI configuration failed\n");
        exit(1);
    }

    runtime_apply(ctx.runtime.environ);
    snprintf(env_name, sizeof(env_name_testing) - 1, "%s", ctx.info.release_name);
    snprintf(env_name_testing, sizeof(env_name) - 1, "%s_test", env_name);

    msg(OMC_MSG_L1, "Overview\n");
    delivery_meta_show(&ctx);
    delivery_conda_show(&ctx);
    delivery_tests_show(&ctx);
    if (globals.verbose) {
        delivery_runtime_show(&ctx);
    }

    msg(OMC_MSG_L1, "Conda setup\n");
    delivery_get_installer_url(&ctx, installer_url);
    msg(OMC_MSG_L2, "Downloading: %s\n", installer_url);
    if (delivery_get_installer(installer_url)) {
        msg(OMC_MSG_ERROR, "download failed: %s\n", installer_url);
        exit(1);
    }

    // Unlikely to occur: this should help prevent rmtree() from destroying your entire filesystem
    // if path is "/" then, die
    // or if empty string, die
    if (!strcmp(ctx.storage.conda_install_prefix, DIR_SEP) || !strlen(ctx.storage.conda_install_prefix)) {
        fprintf(stderr, "error: ctx.storage.conda_install_prefix is malformed!\n");
        exit(1);
    }

    msg(OMC_MSG_L2, "Installing: %s\n", path_basename(installer_url));
    delivery_install_conda(path_basename(installer_url), ctx.storage.conda_install_prefix);

    msg(OMC_MSG_L2, "Configuring: %s\n", ctx.storage.conda_install_prefix);
    delivery_conda_enable(&ctx, ctx.storage.conda_install_prefix);

    char *pathvar = NULL;
    pathvar = getenv("PATH");
    if (!pathvar) {
        msg(OMC_MSG_ERROR | OMC_MSG_L2, "PATH variable is not set. Cannot continue.\n");
        exit(1);
    } else {
        char pathvar_tmp[OMC_BUFSIZ];
        sprintf(pathvar_tmp, "%s/bin:%s", ctx.storage.conda_install_prefix, pathvar);
        setenv("PATH", pathvar_tmp, 1);
    }

    msg(OMC_MSG_L1, "Creating release environment(s)\n");
    if (ctx.meta.based_on && strlen(ctx.meta.based_on)) {
        if (conda_env_remove(env_name)) {
           msg(OMC_MSG_ERROR | OMC_MSG_L2, "failed to remove release environment: %s\n", env_name_testing);
           exit(1);
        }
        msg(OMC_MSG_L2, "Based on release: %s\n", ctx.meta.based_on);
        if (conda_env_create_from_uri(env_name, ctx.meta.based_on)) {
            msg(OMC_MSG_ERROR | OMC_MSG_L2, "unable to install release environment using configuration file\n");
            exit(1);
        }

        if (conda_env_remove(env_name_testing)) {
            msg(OMC_MSG_ERROR | OMC_MSG_L2, "failed to remove testing environment\n");
            exit(1);
        }
        if (conda_env_create_from_uri(env_name_testing, ctx.meta.based_on)) {
            msg(OMC_MSG_ERROR | OMC_MSG_L2, "unable to install testing environment using configuration file\n");
            exit(1);
        }
    } else {
        if (conda_env_create(env_name, ctx.meta.python, NULL)) {
            msg(OMC_MSG_ERROR | OMC_MSG_L2, "failed to create release environment\n");
            exit(1);
        }
        if (conda_env_create(env_name_testing, ctx.meta.python, NULL)) {
            msg(OMC_MSG_ERROR | OMC_MSG_L2, "failed to create release environment\n");
            exit(1);
        }
    }

    // Activate test environment
    msg(OMC_MSG_L1, "Activating test environment\n");
    if (conda_activate(ctx.storage.conda_install_prefix, env_name_testing)) {
        fprintf(stderr, "failed to activate test environment\n");
        exit(1);
    }

    delivery_gather_tool_versions(&ctx);
    if (!ctx.conda.tool_version) {
        msg(OMC_MSG_ERROR | OMC_MSG_L2, "Could not determine conda version\n");
        exit(1);
    }
    if (!ctx.conda.tool_build_version) {
        msg(OMC_MSG_ERROR | OMC_MSG_L2, "Could not determine conda-build version\n");
        exit(1);
    }

    if (pip_exec("install build")) {
        msg(OMC_MSG_ERROR | OMC_MSG_L2, "'build' tool installation failed");
        exit(1);
    }

    // Execute configuration-defined tests
    msg(OMC_MSG_L1, "Begin test execution\n");
    delivery_tests_run(&ctx);

    msg(OMC_MSG_L1, "Generating deferred package listing\n");
    // Test succeeded so move on to producing package artifacts
    delivery_defer_packages(&ctx, DEFER_CONDA);
    delivery_defer_packages(&ctx, DEFER_PIP);

    if (ctx.conda.conda_packages_defer && strlist_count(ctx.conda.conda_packages_defer)) {
        msg(OMC_MSG_L2, "Building Conda recipe(s)\n");
        if (delivery_build_recipes(&ctx)) {
            exit(1);
        }
        msg(OMC_MSG_L3, "Copying artifacts\n");
        if (delivery_copy_conda_artifacts(&ctx)) {
            exit(1);
        }
        msg(OMC_MSG_L3, "Indexing artifacts\n");
        if (delivery_index_conda_artifacts(&ctx)) {
            exit(1);
        }
    }

    // Populate the release environment
    msg(OMC_MSG_L1, "Populating release environment\n");
    msg(OMC_MSG_L2, "Installing conda packages\n");
    if (strlist_count(ctx.conda.conda_packages)) {
        if (delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_CONDA, (struct StrList *[]) {ctx.conda.conda_packages, NULL})) {
            exit(1);
        }
    }
    if (strlist_count(ctx.conda.conda_packages_defer)) {
        msg(OMC_MSG_L3, "Installing deferred conda packages\n");
        if (delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_CONDA | INSTALL_PKG_CONDA_DEFERRED, (struct StrList *[]) {ctx.conda.conda_packages_defer, NULL})) {
            exit(1);
        }
    } else {
        msg(OMC_MSG_L3, "No deferred conda packages\n");
    }

    msg(OMC_MSG_L2, "Installing pip packages\n");
    if (strlist_count(ctx.conda.pip_packages)) {
        delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_PIP, (struct StrList *[]) {ctx.conda.pip_packages, NULL});
    }

    msg(OMC_MSG_L3, "Installing deferred pip packages\n");
    if (strlist_count(ctx.conda.pip_packages_defer)) {
        // TODO: wheels would be nice, but can't right now
        if (!delivery_build_wheels(&ctx)) {
            exit(1);
        }
        if (delivery_copy_wheel_artifacts(&ctx)) {
            exit(1);
        }
        if (delivery_index_wheel_artifacts(&ctx)) {
            exit(1);
        }

        delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_PIP | INSTALL_PKG_PIP_DEFERRED, (struct StrList *[]) {ctx.conda.pip_packages_defer, NULL});
    }


    conda_exec("list");

    msg(OMC_MSG_L1, "Creating release\n");
    msg(OMC_MSG_L2, "Exporting %s\n", env_name_testing);
    if (conda_env_export(env_name_testing, ctx.storage.delivery_dir, env_name_testing)) {
        msg(OMC_MSG_ERROR | OMC_MSG_L2, "failed %s\n", env_name_testing);
        exit(1);
    }

    msg(OMC_MSG_L2, "Exporting %s\n", env_name);
    if (conda_env_export(env_name, ctx.storage.delivery_dir, env_name)) {
        msg(OMC_MSG_ERROR | OMC_MSG_L2, "failed %s\n", env_name);
        exit(1);
    }

    // Rewrite release environment output (i.e. set package origin(s) to point to the deployment server, etc.)
    char specfile[PATH_MAX];
    sprintf(specfile, "%s/%s.yml", ctx.storage.delivery_dir, env_name);
    msg(OMC_MSG_L3, "Rewriting release spec file %s\n", path_basename(specfile));
    delivery_rewrite_spec(&ctx, specfile);

    msg(OMC_MSG_L1, "Uploading artifacts\n");
    delivery_artifact_upload(&ctx);

    msg(OMC_MSG_L1, "Cleaning up\n");
    ini_free(&ini);
    if (cfg) {
        // optional extras
        ini_free(&cfg);
    }
    delivery_free(&ctx);
    globals_free();
    tpl_free();

    msg(OMC_MSG_L1, "Done!\n");
    return 0;
}

