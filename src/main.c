#define GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/utsname.h>
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
                     "                     Delivery Generator                              \n"
                     "                           v%s\n"
                     "---------------------------------------------------------------------\n"
                     "Copyright (C) 2023 %s,\n"
                     "Association of Universities for Research in Astronomy (AURA)\n";

struct OMC_GLOBAL globals = {
    .verbose = 0,
    .continue_on_error = 0,
    .always_update_base_environment = 0,
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
        char tmp[255] = {0};
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

        sprintf(tmp, "\t%%-%ds\t%%s\t\t%%s", width + 4);
        sprintf(output, tmp, opt_long, opt_short, long_options_help[x]);
        puts(output);
    }
}

int main(int argc, char *argv[], char *arge[]) {
    struct INIFILE *cfg = NULL;
    struct INIFILE *ini = NULL;
    struct Delivery ctx;
    struct Process proc = {
            .stdout = "",
            .stderr = "",
            .redirect_stderr = 0,
    };
    char env_name[PATH_MAX];
    char env_name_testing[PATH_MAX];
    char *delivery_input = argv[1];
    char *config_input = argv[2];
    char installer_url[PATH_MAX];
    char python_override_version[NAME_MAX];
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

    msg(OMC_MSG_L1, "Initializing\n");
    struct utsname uts;
    uname(&uts);

    msg(OMC_MSG_L2, "Setting architecture\n");
    char archsuffix[255];
    ctx.system.arch = strdup(uts.machine);
    if (!strcmp(ctx.system.arch, "x86_64")) {
        strcpy(archsuffix, "64");
    } else {
        strcpy(archsuffix, ctx.system.arch);
    }

    msg(OMC_MSG_L2, "Setting platform\n");
    strcpy(ctx.system.platform[DELIVERY_PLATFORM], uts.sysname);
    if (!strcmp(ctx.system.platform[DELIVERY_PLATFORM], "Darwin")) {
        sprintf(ctx.system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], "osx-%s", archsuffix);
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], "MacOSX");
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_RELEASE], "macos");
    } else if (!strcmp(ctx.system.platform[DELIVERY_PLATFORM], "Linux")) {
        sprintf(ctx.system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], "linux-%s", archsuffix);
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], "Linux");
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_RELEASE], "linux");
    } else {
        // Not explicitly supported systems
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], ctx.system.platform[DELIVERY_PLATFORM]);
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], ctx.system.platform[DELIVERY_PLATFORM]);
        strcpy(ctx.system.platform[DELIVERY_PLATFORM_RELEASE], ctx.system.platform[DELIVERY_PLATFORM]);
        tolower_s(ctx.system.platform[DELIVERY_PLATFORM_RELEASE]);
    }

    msg(OMC_MSG_L2, "Setting up runtime environment...\n");
    setenv("OMC_ARCH", ctx.system.arch, 1);
    setenv("OMC_PLATFORM", ctx.system.platform[DELIVERY_PLATFORM], 1);
    setenv("OMC_CONDA_ARCH", ctx.system.arch, 1);
    setenv("OMC_CONDA_PLATFORM", ctx.system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], 1);
    setenv("OMC_CONDA_PLATFORM_SUBDIR", ctx.system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], 1);

    if (config_input) {
        msg(OMC_MSG_L2, "Reading OMC global configuration: %s\n", config_input);
        cfg = ini_open(config_input);
        //ini_show(cfg);
    }

    msg(OMC_MSG_L2, "Reading OMC delivery configuration: %s\n", delivery_input);
    ini = ini_open(delivery_input);

    printf(BANNER, VERSION, AUTHOR);

    delivery_init(&ctx, ini, cfg);
    runtime_apply(ctx.runtime.environ);
    sprintf(env_name, "%s", ctx.info.release_name);
    sprintf(env_name_testing, "%s_test", env_name);

    msg(OMC_MSG_L1, "Overview\n");
    delivery_meta_show(&ctx);
    delivery_conda_show(&ctx);
    delivery_tests_show(&ctx);

    msg(OMC_MSG_L1, "Conda setup\n");
    delivery_get_installer_url(&ctx, installer_url);
    msg(OMC_MSG_L2, "Downloading: %s\n", installer_url);
    delivery_get_installer(installer_url);

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

    msg(OMC_MSG_L1, "Creating release environment(s)\n");
    if (ctx.meta.based_on && strlen(ctx.meta.based_on)) {
        conda_env_remove(env_name);
        conda_env_create_from_uri(env_name, ctx.meta.based_on);

        conda_env_remove(env_name_testing);
        conda_env_create_from_uri(env_name_testing, ctx.meta.based_on);
    } else {
        conda_env_create(env_name, ctx.meta.python, NULL);
        conda_env_create(env_name_testing, ctx.meta.python, NULL);
    }

    // Activate test environment
    msg(OMC_MSG_L1, "Activating test environment\n");
    if (conda_activate(ctx.storage.conda_install_prefix, env_name_testing)) {
        fprintf(stderr, "failed to activate test environment\n");
        exit(1);
    }

    msg(OMC_MSG_L2, "Installing build tools\n");
    if (conda_exec("install boa conda-build conda-verify")) {
        msg(OMC_MSG_ERROR | OMC_MSG_L2, "conda-build installation failed");
        exit(1);
    }

    // TODO: extract version from tool output
    ctx.conda.tool_version = NULL;
    ctx.conda.tool_build_version = NULL;

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

    // TODO: wheels would be nice, but can't right now
    //if (ctx.conda.pip_packages_defer) {
    //    if (!delivery_build_wheels(&ctx)) {
    //        exit(1);
    //    }
    //    if (delivery_copy_wheel_artifacts(&ctx)) {
    //        exit(1);
    //    }
    //    if (delivery_index_wheel_artifacts(&ctx)) {
    //        exit(1);
    //    }
    //}

    if (ctx.conda.conda_packages_defer) {
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
    delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_CONDA, (struct StrList *[]) {ctx.conda.conda_packages, NULL});
    msg(OMC_MSG_L3, "Installing deferred conda packages\n");
    delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_CONDA | INSTALL_PKG_CONDA_DEFERRED, (struct StrList *[]) {ctx.conda.conda_packages_defer, NULL});
    msg(OMC_MSG_L2, "Installing pip packages\n");
    delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_PIP, (struct StrList *[]) {ctx.conda.pip_packages, NULL});
    msg(OMC_MSG_L3, "Installing deferred pip packages\n");
    delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_PIP | INSTALL_PKG_PIP_DEFERRED, (struct StrList *[]) {ctx.conda.pip_packages_defer, NULL});

    conda_exec("list");

    msg(OMC_MSG_L1, "Creating release\n");
    msg(OMC_MSG_L2, "Exporting %s\n", env_name_testing);
    conda_env_export(env_name_testing, ctx.storage.delivery_dir, env_name_testing);

    msg(OMC_MSG_L2, "Exporting %s\n", env_name);
    conda_env_export(env_name, ctx.storage.delivery_dir, env_name);

    // Rewrite release environment output (i.e. set package origin(s) to point to the deployment server, etc.)
    char specfile[PATH_MAX];
    sprintf(specfile, "%s/%s.yml", ctx.storage.delivery_dir, env_name);
    msg(OMC_MSG_L3, "Rewriting release file %s\n", path_basename(specfile));
    delivery_rewrite_spec(&ctx, specfile);

    msg(OMC_MSG_L1, "Cleaning up\n");
    ini_free(&ini);
    if (cfg) {
        ini_free(&cfg);
    }
    delivery_free(&ctx);

    msg(OMC_MSG_L1, "Done!\n");
    return 0;
}

