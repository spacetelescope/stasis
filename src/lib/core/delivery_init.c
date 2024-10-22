#include "delivery.h"

int has_mount_flags(const char *mount_point, const unsigned long flags) {
    struct statvfs st;
    if (statvfs(mount_point, &st)) {
        SYSERROR("Unable to determine mount-point flags: %s", strerror(errno));
        return -1;
    }
    return (st.f_flag & flags) != 0;
}

int delivery_init_tmpdir(struct Delivery *ctx) {
    char *tmpdir = NULL;
    char *x = NULL;
    int unusable = 0;
    errno = 0;

    x = getenv("TMPDIR");
    if (x) {
        guard_free(ctx->storage.tmpdir);
        tmpdir = strdup(x);
    } else {
        tmpdir = ctx->storage.tmpdir;
    }

    if (!tmpdir) {
        // memory error
        return -1;
    }

    // If the directory doesn't exist, create it
    if (access(tmpdir, F_OK) < 0) {
        if (mkdirs(tmpdir, 0755) < 0) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "Unable to create temporary storage directory: %s (%s)\n", tmpdir, strerror(errno));
            goto l_delivery_init_tmpdir_fatal;
        }
    }

    // If we can't read, write, or execute, then die
    if (access(tmpdir, R_OK | W_OK | X_OK) < 0) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "%s requires at least 0755 permissions.\n");
        goto l_delivery_init_tmpdir_fatal;
    }

    struct statvfs st;
    if (statvfs(tmpdir, &st) < 0) {
        goto l_delivery_init_tmpdir_fatal;
    }

#if defined(STASIS_OS_LINUX)
    // If we can't execute programs, or write data to the file system at all, then die
    if ((st.f_flag & ST_NOEXEC) != 0) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "%s is mounted with noexec\n", tmpdir);
        goto l_delivery_init_tmpdir_fatal;
    }
#endif
    if ((st.f_flag & ST_RDONLY) != 0) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "%s is mounted read-only\n", tmpdir);
        goto l_delivery_init_tmpdir_fatal;
    }

    if (!globals.tmpdir) {
        globals.tmpdir = strdup(tmpdir);
    }

    if (!ctx->storage.tmpdir) {
        ctx->storage.tmpdir = strdup(globals.tmpdir);
    }
    return unusable;

    l_delivery_init_tmpdir_fatal:
    unusable = 1;
    return unusable;
}

void delivery_init_dirs_stage2(struct Delivery *ctx) {
    path_store(&ctx->storage.build_recipes_dir, PATH_MAX, ctx->storage.build_dir, "recipes");
    path_store(&ctx->storage.build_sources_dir, PATH_MAX, ctx->storage.build_dir, "sources");
    path_store(&ctx->storage.build_testing_dir, PATH_MAX, ctx->storage.build_dir, "testing");
    path_store(&ctx->storage.build_docker_dir, PATH_MAX, ctx->storage.build_dir, "docker");

    path_store(&ctx->storage.delivery_dir, PATH_MAX, ctx->storage.output_dir, "delivery");
    path_store(&ctx->storage.results_dir, PATH_MAX, ctx->storage.output_dir, "results");
    path_store(&ctx->storage.package_dir, PATH_MAX, ctx->storage.output_dir, "packages");
    path_store(&ctx->storage.cfgdump_dir, PATH_MAX, ctx->storage.output_dir, "config");
    path_store(&ctx->storage.meta_dir, PATH_MAX, ctx->storage.output_dir, "meta");

    path_store(&ctx->storage.conda_artifact_dir, PATH_MAX, ctx->storage.package_dir, "conda");
    path_store(&ctx->storage.wheel_artifact_dir, PATH_MAX, ctx->storage.package_dir, "wheels");
    path_store(&ctx->storage.docker_artifact_dir, PATH_MAX, ctx->storage.package_dir, "docker");
}

void delivery_init_dirs_stage1(struct Delivery *ctx) {
    char *rootdir = getenv("STASIS_ROOT");
    if (rootdir) {
        if (isempty(rootdir)) {
            fprintf(stderr, "STASIS_ROOT is set, but empty. Please assign a file system path to this environment variable.\n");
            exit(1);
        }
        path_store(&ctx->storage.root, PATH_MAX, rootdir, ctx->info.build_name);
    } else {
        // use "stasis" in current working directory
        path_store(&ctx->storage.root, PATH_MAX, "stasis", ctx->info.build_name);
    }
    path_store(&ctx->storage.tools_dir, PATH_MAX, ctx->storage.root, "tools");
    path_store(&ctx->storage.tmpdir, PATH_MAX, ctx->storage.root, "tmp");
    if (delivery_init_tmpdir(ctx)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "Set $TMPDIR to a location other than %s\n", globals.tmpdir);
        if (globals.tmpdir)
            guard_free(globals.tmpdir);
        exit(1);
    }

    path_store(&ctx->storage.build_dir, PATH_MAX, ctx->storage.root, "build");
    path_store(&ctx->storage.output_dir, PATH_MAX, ctx->storage.root, "output");

    if (!ctx->storage.mission_dir) {
        path_store(&ctx->storage.mission_dir, PATH_MAX, globals.sysconfdir, "mission");
    }

    if (access(ctx->storage.mission_dir, F_OK)) {
        msg(STASIS_MSG_L1, "%s: %s\n", ctx->storage.mission_dir, strerror(errno));
        exit(1);
    }

    // Override installation prefix using global configuration key
    if (globals.conda_install_prefix && strlen(globals.conda_install_prefix)) {
        // user wants a specific path
        globals.conda_fresh_start = false;
        /*
        if (mkdirs(globals.conda_install_prefix, 0755)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "Unable to create directory: %s: %s\n",
                strerror(errno), globals.conda_install_prefix);
            exit(1);
        }
         */
        /*
        ctx->storage.conda_install_prefix = realpath(globals.conda_install_prefix, NULL);
        if (!ctx->storage.conda_install_prefix) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "realpath(): Conda installation prefix reassignment failed\n");
            exit(1);
        }
        ctx->storage.conda_install_prefix = strdup(globals.conda_install_prefix);
         */
        path_store(&ctx->storage.conda_install_prefix, PATH_MAX, globals.conda_install_prefix, "conda");
    } else {
        // install conda under the STASIS tree
        path_store(&ctx->storage.conda_install_prefix, PATH_MAX, ctx->storage.tools_dir, "conda");
    }
}

int delivery_init_platform(struct Delivery *ctx) {
    msg(STASIS_MSG_L2, "Setting architecture\n");
    char archsuffix[20];
    struct utsname uts;
    if (uname(&uts)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "uname() failed: %s\n", strerror(errno));
        return -1;
    }

    ctx->system.platform = calloc(DELIVERY_PLATFORM_MAX + 1, sizeof(*ctx->system.platform));
    if (!ctx->system.platform) {
        SYSERROR("Unable to allocate %d records for platform array\n", DELIVERY_PLATFORM_MAX);
        return -1;
    }
    for (size_t i = 0; i < DELIVERY_PLATFORM_MAX; i++) {
        ctx->system.platform[i] = calloc(DELIVERY_PLATFORM_MAXLEN, sizeof(*ctx->system.platform[0]));
    }

    ctx->system.arch = strdup(uts.machine);
    if (!ctx->system.arch) {
        // memory error
        return -1;
    }

    if (!strcmp(ctx->system.arch, "x86_64")) {
        strcpy(archsuffix, "64");
    } else {
        strcpy(archsuffix, ctx->system.arch);
    }

    msg(STASIS_MSG_L2, "Setting platform\n");
    strcpy(ctx->system.platform[DELIVERY_PLATFORM], uts.sysname);
    if (!strcmp(ctx->system.platform[DELIVERY_PLATFORM], "Darwin")) {
        sprintf(ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], "osx-%s", archsuffix);
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], "MacOSX");
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_RELEASE], "macos");
    } else if (!strcmp(ctx->system.platform[DELIVERY_PLATFORM], "Linux")) {
        sprintf(ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], "linux-%s", archsuffix);
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], "Linux");
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_RELEASE], "linux");
    } else {
        // Not explicitly supported systems
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], ctx->system.platform[DELIVERY_PLATFORM]);
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], ctx->system.platform[DELIVERY_PLATFORM]);
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_RELEASE], ctx->system.platform[DELIVERY_PLATFORM]);
        tolower_s(ctx->system.platform[DELIVERY_PLATFORM_RELEASE]);
    }

    long cpu_count = get_cpu_count();
    if (!cpu_count) {
        fprintf(stderr, "Unable to determine CPU count. Falling back to 1.\n");
        cpu_count = 1;
    }
    char ncpus[100] = {0};
    sprintf(ncpus, "%ld", cpu_count);

    // Declare some important bits as environment variables
    setenv("CPU_COUNT", ncpus, 1);
    setenv("STASIS_CPU_COUNT", ncpus, 1);
    setenv("STASIS_ARCH", ctx->system.arch, 1);
    setenv("STASIS_PLATFORM", ctx->system.platform[DELIVERY_PLATFORM], 1);
    setenv("STASIS_CONDA_ARCH", ctx->system.arch, 1);
    setenv("STASIS_CONDA_PLATFORM", ctx->system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], 1);
    setenv("STASIS_CONDA_PLATFORM_SUBDIR", ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], 1);

    // Register template variables
    // These were moved out of main() because we can't take the address of system.platform[x]
    // _before_ the array has been initialized.
    tpl_register("system.arch", &ctx->system.arch);
    tpl_register("system.platform", &ctx->system.platform[DELIVERY_PLATFORM_RELEASE]);

    return 0;
}

int delivery_init(struct Delivery *ctx, int render_mode) {
    populate_info(ctx);
    populate_delivery_cfg(ctx, INI_READ_RENDER);

    // Set artifactory URL via environment variable if possible
    char *jfurl = getenv("STASIS_JF_ARTIFACTORY_URL");
    if (jfurl) {
        if (globals.jfrog.url) {
            guard_free(globals.jfrog.url);
        }
        globals.jfrog.url = strdup(jfurl);
    }

    // Set artifactory repository via environment if possible
    char *jfrepo = getenv("STASIS_JF_REPO");
    if (jfrepo) {
        if (globals.jfrog.repo) {
            guard_free(globals.jfrog.repo);
        }
        globals.jfrog.repo = strdup(jfrepo);
    }

    // Configure architecture and platform information
    delivery_init_platform(ctx);

    // Create STASIS directory structure
    delivery_init_dirs_stage1(ctx);

    char config_local[PATH_MAX];
    sprintf(config_local, "%s/%s", ctx->storage.tmpdir, "config");
    setenv("XDG_CONFIG_HOME", config_local, 1);

    char cache_local[PATH_MAX];
    sprintf(cache_local, "%s/%s", ctx->storage.tmpdir, "cache");
    setenv("XDG_CACHE_HOME", cache_local, 1);

    // add tools to PATH
    char pathvar_tmp[STASIS_BUFSIZ];
    sprintf(pathvar_tmp, "%s/bin:%s", ctx->storage.tools_dir, getenv("PATH"));
    setenv("PATH", pathvar_tmp, 1);

    // Prevent git from paginating output
    setenv("GIT_PAGER", "", 1);

    populate_delivery_ini(ctx, render_mode);

    if (ctx->deploy.docker.tags) {
        for (size_t i = 0; i < strlist_count(ctx->deploy.docker.tags); i++) {
            char *item = strlist_item(ctx->deploy.docker.tags, i);
            tolower_s(item);
        }
    }

    if (ctx->deploy.docker.image_compression) {
        if (docker_validate_compression_program(ctx->deploy.docker.image_compression)) {
            SYSERROR("[deploy:docker].image_compression - invalid command / program is not installed: %s", ctx->deploy.docker.image_compression);
            return -1;
        }
    }
    return 0;
}

int bootstrap_build_info(struct Delivery *ctx) {
    struct Delivery local;
    memset(&local, 0, sizeof(local));
    local._stasis_ini_fp.cfg = ini_open(ctx->_stasis_ini_fp.cfg_path);
    local._stasis_ini_fp.delivery = ini_open(ctx->_stasis_ini_fp.delivery_path);
    delivery_init_platform(&local);
    populate_delivery_cfg(&local, INI_READ_RENDER);
    populate_delivery_ini(&local, INI_READ_RENDER);
    populate_info(&local);
    ctx->info.build_name = strdup(local.info.build_name);
    ctx->info.build_number = strdup(local.info.build_number);
    ctx->info.release_name = strdup(local.info.release_name);
    ctx->info.time_info = malloc(sizeof(*ctx->info.time_info));
    if (!ctx->info.time_info) {
        SYSERROR("Unable to allocate %zu bytes for tm struct: %s", sizeof(*local.info.time_info), strerror(errno));
        return -1;
    }
    memcpy(ctx->info.time_info, local.info.time_info, sizeof(*local.info.time_info));
    ctx->info.time_now = local.info.time_now;
    ctx->info.time_str_epoch = strdup(local.info.time_str_epoch);
    delivery_free(&local);
    return 0;
}

int delivery_exists(struct Delivery *ctx) {
    int release_exists = 0;
    char release_pattern[PATH_MAX] = {0};
    sprintf(release_pattern, "*%s*", ctx->info.release_name);

    if (globals.enable_artifactory) {
        if (jfrt_auth_init(&ctx->deploy.jfrog_auth)) {
            fprintf(stderr, "Failed to initialize Artifactory authentication context\n");
            return -1;  // error
        }

        struct JFRT_Search search = {.fail_no_op = true};
        release_exists = jfrog_cli_rt_search(&ctx->deploy.jfrog_auth, &search, globals.jfrog.repo, release_pattern);
        if (release_exists != 2) {
            if (!globals.enable_overwrite && !release_exists) {
                // --fail_no_op returns 2 on failure
                // without: it returns an empty list "[]" and exit code 0
                return 1;  // found
            }
        }
    } else {
        struct StrList *files = listdir(ctx->storage.delivery_dir);
        for (size_t i = 0; i < strlist_count(files); i++) {
            char *filename = strlist_item(files, i);
            release_exists = fnmatch(release_pattern, filename, FNM_PATHNAME);
            if (!globals.enable_overwrite && !release_exists) {
                guard_strlist_free(&files);
                return 1;  // found
            }
        }
        guard_strlist_free(&files);
    }
    return 0;  // not found
}
