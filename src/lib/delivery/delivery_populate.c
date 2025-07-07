#include "delivery.h"

static void ini_has_key_required(struct INIFILE *ini, const char *section_name, char *key) {
    int status = ini_has_key(ini, section_name, key);
    if (!status) {
        SYSERROR("%s:%s key is required but not defined", section_name, key);
        exit(1);
    }
}

static void conv_str(char **x, union INIVal val) {
    if (*x) {
        guard_free(*x);
    }
    if (val.as_char_p) {
        char *tplop = tpl_render(val.as_char_p);
        if (tplop) {
            *x = tplop;
        } else {
            *x = NULL;
        }
    } else {
        *x = NULL;
    }
}



int populate_info(struct Delivery *ctx) {
    if (!ctx->info.time_str_epoch) {
        // Record timestamp used for release
        time(&ctx->info.time_now);
        if (!ctx->info.time_info) {
            ctx->info.time_info = malloc(sizeof(*ctx->info.time_info));
            if (!ctx->info.time_info) {
                msg(STASIS_MSG_ERROR, "%s: Unable to allocate memory for time_info\n", strerror(errno));
                return -1;
            }
            if (!localtime_r(&ctx->info.time_now, ctx->info.time_info)) {
                msg(STASIS_MSG_ERROR, "%s: localtime_r failed\n", strerror(errno));
                return -1;
            }
        }

        ctx->info.time_str_epoch = calloc(STASIS_TIME_STR_MAX, sizeof(*ctx->info.time_str_epoch));
        if (!ctx->info.time_str_epoch) {
            msg(STASIS_MSG_ERROR, "%s: Unable to allocate memory for Unix epoch string\n", strerror(errno));
            return -1;
        }
        snprintf(ctx->info.time_str_epoch, STASIS_TIME_STR_MAX - 1, "%li", ctx->info.time_now);
    }
    return 0;
}

int populate_delivery_cfg(struct Delivery *ctx, int render_mode) {
    struct INIFILE *cfg = ctx->_stasis_ini_fp.cfg;
    if (!cfg) {
        return -1;
    }
    int err = 0;
    ctx->storage.conda_staging_dir = ini_getval_str(cfg, "default", "conda_staging_dir", render_mode, &err);
    ctx->storage.conda_staging_url = ini_getval_str(cfg, "default", "conda_staging_url", render_mode, &err);
    ctx->storage.wheel_staging_dir = ini_getval_str(cfg, "default", "wheel_staging_dir", render_mode, &err);
    ctx->storage.wheel_staging_url = ini_getval_str(cfg, "default", "wheel_staging_url", render_mode, &err);
    globals.conda_fresh_start = ini_getval_bool(cfg, "default", "conda_fresh_start", render_mode, &err);
    if (!globals.continue_on_error) {
        globals.continue_on_error = ini_getval_bool(cfg, "default", "continue_on_error", render_mode, &err);
    }
    if (!globals.always_update_base_environment) {
        globals.always_update_base_environment = ini_getval_bool(cfg, "default", "always_update_base_environment", render_mode, &err);
    }
    if (globals.conda_install_prefix) {
        guard_free(globals.conda_install_prefix);
    }
    globals.conda_install_prefix = ini_getval_str(cfg, "default", "conda_install_prefix", render_mode, &err);

    if (globals.conda_packages) {
        guard_strlist_free(&globals.conda_packages);
    }
    globals.conda_packages = ini_getval_strlist(cfg, "default", "conda_packages", LINE_SEP, render_mode, &err);

    if (globals.pip_packages) {
        guard_strlist_free(&globals.pip_packages);
    }
    globals.pip_packages = ini_getval_strlist(cfg, "default", "pip_packages", LINE_SEP, render_mode, &err);

    if (globals.jfrog.jfrog_artifactory_base_url) {
        guard_free(globals.jfrog.jfrog_artifactory_base_url);
    }
    globals.jfrog.jfrog_artifactory_base_url = ini_getval_str(cfg, "jfrog_cli_download", "url", render_mode, &err);

    if (globals.jfrog.jfrog_artifactory_product) {
        guard_free(globals.jfrog.jfrog_artifactory_product);
    }
    globals.jfrog.jfrog_artifactory_product = ini_getval_str(cfg, "jfrog_cli_download", "product", render_mode, &err);

    if (globals.jfrog.cli_major_ver) {
        guard_free(globals.jfrog.cli_major_ver);
    }
    globals.jfrog.cli_major_ver = ini_getval_str(cfg, "jfrog_cli_download", "version_series", render_mode, &err);

    if (globals.jfrog.version) {
        guard_free(globals.jfrog.version);
    }
    globals.jfrog.version = ini_getval_str(cfg, "jfrog_cli_download", "version", render_mode, &err);

    if (globals.jfrog.remote_filename) {
        guard_free(globals.jfrog.remote_filename);
    }
    globals.jfrog.remote_filename = ini_getval_str(cfg, "jfrog_cli_download", "filename", render_mode, &err);

    if (globals.jfrog.url) {
        guard_free(globals.jfrog.url);
    }
    globals.jfrog.url = ini_getval_str(cfg, "deploy:artifactory", "url", render_mode, &err);

    if (globals.jfrog.repo) {
        guard_free(globals.jfrog.repo);
    }
    globals.jfrog.repo = ini_getval_str(cfg, "deploy:artifactory", "repo", render_mode, &err);

    return 0;
}

static void normalize_ini_list(struct INIFILE **inip, struct StrList **listp, char * const section, char * const key, int render_mode) {
    struct INIFILE *ini = *inip;
    struct StrList *list = *listp;
    if (!list) {
        return;
    }
    char **data = list->data;
    if (!data) {
        return;
    }

    if (data[0] && strpbrk(data[0], " \t")) {
        normalize_space(data[0]);
        replace_text(data[0], " ", LINE_SEP, 0);
        char *replacement = join(data, LINE_SEP);
        ini_setval(&ini, INI_SETVAL_REPLACE, section, key, replacement);
        guard_free(replacement);
        guard_strlist_free(&list);
        int err = 0;
        list = ini_getval_strlist(ini, section, key, LINE_SEP, render_mode, &err);
    }

    for (size_t i = 0; i < strlist_count(list); i++) {
        char *pkg = strlist_item(list, i);
        if (strpbrk(pkg, ";#") || isempty(pkg)) {
            strlist_remove(list, i);
        }
    }
    (*inip) = ini;
    (*listp) = list;
}
int populate_delivery_ini(struct Delivery *ctx, int render_mode) {
    struct INIFILE *ini = ctx->_stasis_ini_fp.delivery;
    struct INIData *rtdata;

    validate_delivery_ini(ini);
    // Populate runtime variables first they may be interpreted by other
    // keys in the configuration
    RuntimeEnv *rt = runtime_copy(__environ);
    while ((rtdata = ini_getall(ini, "runtime")) != NULL) {
        runtime_set(rt, rtdata->key, rtdata->value);
    }
    runtime_apply(rt);
    ctx->runtime.environ = rt;

    int err = 0;
    ctx->meta.mission = ini_getval_str(ini, "meta", "mission", render_mode, &err);
    ctx->meta.codename = ini_getval_str(ini, "meta", "codename", render_mode, &err);
    ctx->meta.version = ini_getval_str(ini, "meta", "version", render_mode, &err);
    ctx->meta.name = ini_getval_str(ini, "meta", "name", render_mode, &err);
    ctx->meta.rc = ini_getval_int(ini, "meta", "rc", render_mode, &err);
    ctx->meta.final = ini_getval_bool(ini, "meta", "final", render_mode, &err);
    ctx->meta.based_on = ini_getval_str(ini, "meta", "based_on", render_mode, &err);

    if (!ctx->meta.python) {
        ctx->meta.python = ini_getval_str(ini, "meta", "python", render_mode, &err);
        guard_free(ctx->meta.python_compact);
        ctx->meta.python_compact = to_short_version(ctx->meta.python);
    } else {
        ini_setval(&ini, INI_SETVAL_REPLACE, "meta", "python", ctx->meta.python);
    }

    ctx->conda.installer_name = ini_getval_str(ini, "conda", "installer_name", render_mode, &err);
    ctx->conda.installer_version = ini_getval_str(ini, "conda", "installer_version", render_mode, &err);
    ctx->conda.installer_platform = ini_getval_str(ini, "conda", "installer_platform", render_mode, &err);
    ctx->conda.installer_arch = ini_getval_str(ini, "conda", "installer_arch", render_mode, &err);
    ctx->conda.installer_baseurl = ini_getval_str(ini, "conda", "installer_baseurl", render_mode, &err);
    ctx->conda.conda_packages = ini_getval_strlist(ini, "conda", "conda_packages", LINE_SEP, render_mode, &err);
    normalize_ini_list(&ini, &ctx->conda.conda_packages, "conda", "conda_packages", render_mode);
    ctx->conda.conda_packages_purge = ini_getval_strlist(ini, "conda", "conda_packages_purge", LINE_SEP, render_mode, &err);
    normalize_ini_list(&ini, &ctx->conda.conda_packages_purge, "conda", "conda_package_purge", render_mode);
    ctx->conda.pip_packages = ini_getval_strlist(ini, "conda", "pip_packages", LINE_SEP, render_mode, &err);
    normalize_ini_list(&ini, &ctx->conda.pip_packages, "conda", "pip_packages", render_mode);
    ctx->conda.pip_packages_purge = ini_getval_strlist(ini, "conda", "pip_packages_purge", LINE_SEP, render_mode, &err);
    normalize_ini_list(&ini, &ctx->conda.pip_packages_purge, "conda", "pip_packages_purge", render_mode);

    // Delivery metadata consumed
    populate_mission_ini(&ctx, render_mode);

    if (ctx->info.release_name) {
        guard_free(ctx->info.release_name);
        guard_free(ctx->info.build_name);
        guard_free(ctx->info.build_number);
    }

    // Fail if a mission requires a codename, but one is not defined
    if (strstr(ctx->rules.release_fmt, "%c") && isempty(ctx->meta.codename)) {
        SYSERROR("Mission type '%s' requires meta.codename to be defined", ctx->meta.mission);
        return -1;
    }

    if (delivery_format_str(ctx, &ctx->info.release_name, ctx->rules.release_fmt)) {
        fprintf(stderr, "Failed to generate release name. Format used: %s\n", ctx->rules.release_fmt);
        return -1;
    }

    if (!ctx->info.build_name) {
        delivery_format_str(ctx, &ctx->info.build_name, ctx->rules.build_name_fmt);
    }
    if (!ctx->info.build_number) {
        delivery_format_str(ctx, &ctx->info.build_number, ctx->rules.build_number_fmt);
    }

    // Best I can do to make output directories unique. Annoying.
    delivery_init_dirs_stage2(ctx);

    if (!ctx->conda.conda_packages_defer) {
        ctx->conda.conda_packages_defer = strlist_init();
    }
    if (!ctx->conda.pip_packages_defer) {
        ctx->conda.pip_packages_defer = strlist_init();
    }

    for (size_t z = 0, i = 0; i < ini->section_count; i++) {
        char *section_name = ini->section[i]->key;
        if (startswith(section_name, "test:")) {
            union INIVal val;
            struct Test *test = &ctx->tests[z];
            val.as_char_p = strchr(ini->section[i]->key, ':') + 1;
            if (val.as_char_p && isempty(val.as_char_p)) {
                return 1;
            }
            conv_str(&test->name, val);

            test->version = ini_getval_str(ini, section_name, "version", render_mode, &err);
            test->repository = ini_getval_str(ini, section_name, "repository", render_mode, &err);
            test->script_setup = ini_getval_str(ini, section_name, "script_setup", INI_READ_RAW, &err);
            test->script = ini_getval_str(ini, section_name, "script", INI_READ_RAW, &err);
            test->disable = ini_getval_bool(ini, section_name, "disable", render_mode, &err);
            test->parallel = ini_getval_bool(ini, section_name, "parallel", render_mode, &err);
            if (err) {
                test->parallel = true;
            }
            test->repository_remove_tags = ini_getval_strlist(ini, section_name, "repository_remove_tags", LINE_SEP, render_mode, &err);
            test->build_recipe = ini_getval_str(ini, section_name, "build_recipe", render_mode, &err);
            test->runtime.environ = ini_getval_strlist(ini, section_name, "runtime", LINE_SEP, render_mode, &err);
            z++;
        }
    }

    for (size_t z = 0, i = 0; i < ini->section_count; i++) {
        char *section_name = ini->section[i]->key;
        struct Deploy *deploy = &ctx->deploy;
        if (startswith(section_name, "deploy:artifactory")) {
            struct JFrog *jfrog = &deploy->jfrog[z];
            // Artifactory base configuration

            jfrog->upload_ctx.workaround_parent_only = ini_getval_bool(ini, section_name, "workaround_parent_only", render_mode, &err);
            jfrog->upload_ctx.exclusions = ini_getval_str(ini, section_name, "exclusions", render_mode, &err);
            jfrog->upload_ctx.explode = ini_getval_bool(ini, section_name, "explode", render_mode, &err);
            jfrog->upload_ctx.recursive = ini_getval_bool(ini, section_name, "recursive", render_mode, &err);
            jfrog->upload_ctx.retries = ini_getval_int(ini, section_name, "retries", render_mode, &err);
            jfrog->upload_ctx.retry_wait_time = ini_getval_int(ini, section_name, "retry_wait_time", render_mode, &err);
            jfrog->upload_ctx.detailed_summary = ini_getval_bool(ini, section_name, "detailed_summary", render_mode, &err);
            jfrog->upload_ctx.quiet = ini_getval_bool(ini, section_name, "quiet", render_mode, &err);
            jfrog->upload_ctx.regexp = ini_getval_bool(ini, section_name, "regexp", render_mode, &err);
            jfrog->upload_ctx.spec = ini_getval_str(ini, section_name, "spec", render_mode, &err);
            jfrog->upload_ctx.flat = ini_getval_bool(ini, section_name, "flat", render_mode, &err);
            jfrog->repo = ini_getval_str(ini, section_name, "repo", render_mode, &err);
            jfrog->dest = ini_getval_str(ini, section_name, "dest", render_mode, &err);
            jfrog->files = ini_getval_strlist(ini, section_name, "files", LINE_SEP, render_mode, &err);
            z++;
        }
    }

    for (size_t i = 0; i < ini->section_count; i++) {
        char *section_name = ini->section[i]->key;
        struct Deploy *deploy = &ctx->deploy;
        if (startswith(ini->section[i]->key, "deploy:docker")) {
            struct Docker *docker = &deploy->docker;

            docker->registry = ini_getval_str(ini, section_name, "registry", render_mode, &err);
            docker->image_compression = ini_getval_str(ini, section_name, "image_compression", render_mode, &err);
            docker->test_script = ini_getval_str(ini, section_name, "test_script", render_mode, &err);
            docker->build_args = ini_getval_strlist(ini, section_name, "build_args", LINE_SEP, render_mode, &err);
            docker->tags = ini_getval_strlist(ini, section_name, "tags", LINE_SEP, render_mode, &err);
        }
    }
    return 0;
}

int populate_mission_ini(struct Delivery **ctx, int render_mode) {
    int err = 0;

    if ((*ctx)->_stasis_ini_fp.mission) {
        return 0;
    }

    // Now populate the rules
    char missionfile[PATH_MAX] = {0};
    if (getenv("STASIS_SYSCONFDIR")) {
        sprintf(missionfile, "%s/%s/%s/%s.ini",
                getenv("STASIS_SYSCONFDIR"), "mission", (*ctx)->meta.mission, (*ctx)->meta.mission);
    } else {
        sprintf(missionfile, "%s/%s/%s/%s.ini",
                globals.sysconfdir, "mission", (*ctx)->meta.mission, (*ctx)->meta.mission);
    }

    msg(STASIS_MSG_L2, "Reading mission configuration: %s\n", missionfile);
    (*ctx)->_stasis_ini_fp.mission = ini_open(missionfile);
    struct INIFILE *ini = (*ctx)->_stasis_ini_fp.mission;
    if (!ini) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Failed to read mission configuration: %s, %s\n", missionfile, strerror(errno));
        exit(1);
    }
    (*ctx)->_stasis_ini_fp.mission_path = strdup(missionfile);

    (*ctx)->rules.release_fmt = ini_getval_str(ini, "meta", "release_fmt", render_mode, &err);

    // Used for setting artifactory build info
    (*ctx)->rules.build_name_fmt = ini_getval_str(ini, "meta", "build_name_fmt", render_mode, &err);

    // Used for setting artifactory build info
    (*ctx)->rules.build_number_fmt = ini_getval_str(ini, "meta", "build_number_fmt", render_mode, &err);
    return 0;
}

void validate_delivery_ini(struct INIFILE *ini) {
    if (!ini) {
        SYSERROR("%s", "INIFILE is NULL!");
        exit(1);
    }
    if (ini_section_search(&ini, INI_SEARCH_EXACT, "meta")) {
        ini_has_key_required(ini, "meta", "name");
        ini_has_key_required(ini, "meta", "version");
        ini_has_key_required(ini, "meta", "rc");
        ini_has_key_required(ini, "meta", "mission");
        ini_has_key_required(ini, "meta", "python");
    } else {
        SYSERROR("%s", "[meta] configuration section is required");
        exit(1);
    }

    if (ini_section_search(&ini, INI_SEARCH_EXACT, "conda")) {
        ini_has_key_required(ini, "conda", "installer_name");
        ini_has_key_required(ini, "conda", "installer_version");
        ini_has_key_required(ini, "conda", "installer_platform");
        ini_has_key_required(ini, "conda", "installer_arch");
    } else {
        SYSERROR("%s", "[conda] configuration section is required");
        exit(1);
    }

    for (size_t i = 0; i < ini->section_count; i++) {
        struct INISection *section = ini->section[i];
        if (section && startswith(section->key, "test:")) {
            char *name = strstr(section->key, ":");
            if (name && strlen(name) > 1) {
                name = &name[1];
            }
            //ini_has_key_required(ini, section->key, "version");
            //ini_has_key_required(ini, section->key, "repository");
            if (globals.enable_testing) {
                ini_has_key_required(ini, section->key, "script");
            }
        }
    }

    if (ini_section_search(&ini, INI_SEARCH_EXACT, "deploy:docker")) {
        // yeah?
    }

    for (size_t i = 0; i < ini->section_count; i++) {
        struct INISection *section = ini->section[i];
        if (section && startswith(section->key, "deploy:artifactory")) {
            ini_has_key_required(ini, section->key, "files");
            ini_has_key_required(ini, section->key, "dest");
        }
    }
}

