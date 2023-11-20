//
// Created by jhunk on 10/5/23.
//

#include "deliverable.h"
#include "str.h"
#include "strlist.h"
#include "wheel.h"
#include "copy.h"

extern struct OMC_GLOBAL globals;

#define getter(XINI, SECTION_NAME, KEY, TYPE) \
    { \
        ini_getval(XINI, SECTION_NAME, KEY, TYPE, &val); \
    }
#define getter_required(XINI, SECTION_NAME, KEY, TYPE) \
    { \
        if (ini_getval(XINI, SECTION_NAME, KEY, TYPE, &val)) { \
            fprintf(stderr, "%s:%s is required but not defined\n", SECTION_NAME, KEY); \
            return -1;\
        } \
    }

#define conv_int(X, DEST) X->DEST = val.as_int;
#define conv_str(X, DEST) X->DEST = runtime_expand_var(NULL, val.as_char_p);
#define conv_str_noexpand(X, DEST) if (val.as_char_p) X->DEST = strdup(val.as_char_p);
#define conv_strlist(X, DEST, TOK) { \
    char *rtevnop = runtime_expand_var(NULL, val.as_char_p); \
    if (!X->DEST) \
        X->DEST = strlist_init(); \
    if (rtevnop) {                   \
        strip(rtevnop); \
        strlist_append_tokenize(X->DEST, rtevnop, TOK); \
        free(rtevnop); \
    } else { \
        rtevnop = NULL; \
    } \
}
#define conv_bool(X, DEST) X->DEST = val.as_bool;
#define guard_runtime_free(X) if (X) { runtime_free(X); X = NULL; }
#define guard_strlist_free(X) if (X) { strlist_free(X); X = NULL; }
#define guard_free(X) if (X) { free(X); X = NULL; }

void delivery_free(struct Delivery *ctx) {
    guard_free(ctx->system.arch);
    guard_free(ctx->meta.name)
    guard_free(ctx->meta.version)
    guard_free(ctx->meta.codename)
    guard_free(ctx->meta.mission)
    guard_free(ctx->meta.python)
    guard_free(ctx->meta.mission)
    guard_free(ctx->meta.python_compact);
    guard_runtime_free(ctx->runtime.environ)
    guard_free(ctx->storage.delivery_dir)
    guard_free(ctx->storage.conda_install_prefix)
    guard_free(ctx->storage.conda_artifact_dir)
    guard_free(ctx->storage.conda_staging_dir)
    guard_free(ctx->storage.conda_staging_url)
    guard_free(ctx->storage.wheel_artifact_dir)
    guard_free(ctx->storage.wheel_staging_dir)
    guard_free(ctx->storage.wheel_staging_url)
    guard_free(ctx->storage.build_dir)
    guard_free(ctx->storage.build_recipes_dir)
    guard_free(ctx->storage.build_sources_dir)
    guard_free(ctx->storage.build_testing_dir)
    guard_free(ctx->conda.installer_baseurl)
    guard_free(ctx->conda.installer_name)
    guard_free(ctx->conda.installer_version)
    guard_free(ctx->conda.installer_platform)
    guard_free(ctx->conda.installer_arch)
    guard_free(ctx->conda.tool_version);
    guard_free(ctx->conda.tool_build_version);
    guard_strlist_free(ctx->conda.conda_packages)
    guard_strlist_free(ctx->conda.conda_packages_defer)
    guard_strlist_free(ctx->conda.pip_packages)
    guard_strlist_free(ctx->conda.pip_packages_defer)

    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        guard_free(ctx->tests[i].name)
        guard_free(ctx->tests[i].version)
        guard_free(ctx->tests[i].repository)
        guard_free(ctx->tests[i].script)
        guard_free(ctx->tests[i].build_recipe)
        // test-specific runtime variables
        guard_runtime_free(ctx->tests[i].runtime.environ)
    }
}

void delivery_init_dirs(struct Delivery *ctx) {
    mkdirs("build/recipes", 0755);
    mkdirs("build/sources", 0755);
    mkdirs("build/testing", 0755);
    ctx->storage.build_dir = realpath("build", NULL);
    ctx->storage.build_recipes_dir = realpath("build/recipes", NULL);
    ctx->storage.build_sources_dir = realpath("build/sources", NULL);
    ctx->storage.build_testing_dir = realpath("build/testing", NULL);

    mkdirs("output/omc", 0755);
    mkdirs("output/packages/conda", 0755);
    mkdirs("output/packages/wheels", 0755);
    ctx->storage.delivery_dir = realpath("output/omc", NULL);
    ctx->storage.conda_artifact_dir = realpath("output/packages/conda", NULL);
    ctx->storage.wheel_artifact_dir = realpath("output/packages/wheels", NULL);

    mkdirs(CONDA_INSTALL_PREFIX, 0755);
    ctx->storage.conda_install_prefix = realpath(CONDA_INSTALL_PREFIX, NULL);
}

int delivery_init(struct Delivery *ctx, struct INIFILE *ini, struct INIFILE *cfg) {
    RuntimeEnv *rt;
    struct INIData *rtdata;
    union INIVal val;

    // Record timestamp used for release
    time(&ctx->info.time_now);
    ctx->info.time_info = localtime(&ctx->info.time_now);

    if (cfg) {
        getter(cfg, "default", "conda_staging_dir", INIVAL_TYPE_STR);
        conv_str(ctx, storage.conda_staging_dir);
        getter(cfg, "default", "conda_staging_url", INIVAL_TYPE_STR);
        conv_str(ctx, storage.conda_staging_url);
        getter(cfg, "default", "wheel_staging_dir", INIVAL_TYPE_STR);
        conv_str(ctx, storage.wheel_staging_dir);
        getter(cfg, "default", "wheel_staging_url", INIVAL_TYPE_STR);
        conv_str(ctx, storage.wheel_staging_url);
        // Below can also be toggled by command-line arguments
        getter(cfg, "default", "continue_on_error", INIVAL_TYPE_BOOL)
        globals.continue_on_error = val.as_bool;
        getter(cfg, "default", "always_update_base_environment", INIVAL_TYPE_BOOL);
        globals.always_update_base_environment = val.as_bool;
    }
    delivery_init_dirs(ctx);

    // Populate runtime variables first they may be interpreted by other
    // keys in the configuration
    rt = runtime_copy(__environ);
    while ((rtdata = ini_getall(ini, "runtime")) != NULL) {
        char rec[OMC_BUFSIZ];
        sprintf(rec, "%s=%s", lstrip(strip(rtdata->key)), lstrip(strip(rtdata->value)));
        runtime_set(rt, rtdata->key, rtdata->value);
    }
    runtime_apply(rt);
    ctx->runtime.environ = rt;

    getter(ini, "meta", "mission", INIVAL_TYPE_STR)
    conv_str(ctx, meta.mission)

    if (!strcasecmp(ctx->meta.mission, "hst")) {
        getter(ini, "meta", "codename", INIVAL_TYPE_STR)
        conv_str(ctx, meta.codename)
    } else {
        ctx->meta.codename = NULL;
    }

    if (!strcasecmp(ctx->meta.mission, "jwst")) {
        getter(ini, "meta", "version", INIVAL_TYPE_STR)
        conv_str(ctx, meta.version)

    } else {
        ctx->meta.version = NULL;
    }

    getter_required(ini, "meta", "name", INIVAL_TYPE_STR)
    conv_str(ctx, meta.name)

    getter(ini, "meta", "rc", INIVAL_TYPE_INT)
    conv_int(ctx, meta.rc)

    getter(ini, "meta", "final", INIVAL_TYPE_BOOL)
    conv_bool(ctx, meta.final)

    getter(ini, "meta", "based_on", INIVAL_TYPE_STR)
    conv_str(ctx, meta.based_on)

    getter(ini, "meta", "python", INIVAL_TYPE_STR)
    conv_str(ctx, meta.python)

    getter_required(ini, "conda", "installer_baseurl", INIVAL_TYPE_STR)
    conv_str(ctx, conda.installer_baseurl)

    getter_required(ini, "conda", "installer_name", INIVAL_TYPE_STR)
    conv_str(ctx, conda.installer_name)

    getter_required(ini, "conda", "installer_version", INIVAL_TYPE_STR)
    conv_str(ctx, conda.installer_version)

    getter_required(ini, "conda", "installer_platform", INIVAL_TYPE_STR)
    conv_str(ctx, conda.installer_platform)

    getter_required(ini, "conda", "installer_arch", INIVAL_TYPE_STR)
    conv_str(ctx, conda.installer_arch)

    getter(ini, "conda", "conda_packages", INIVAL_TYPE_STR_ARRAY)
    conv_strlist(ctx, conda.conda_packages, "\n")

    getter(ini, "conda", "pip_packages", INIVAL_TYPE_STR_ARRAY)
    conv_strlist(ctx, conda.pip_packages, "\n")

    ctx->conda.conda_packages_defer = strlist_init();
    ctx->conda.pip_packages_defer = strlist_init();

    for (size_t z = 0, i = 0; i < ini->section_count; i++ ) {
        if (startswith(ini->section[i]->key, "test:")) {
            val.as_char_p = strchr(ini->section[i]->key, ':') + 1;
            conv_str(ctx, tests[z].name)

            getter(ini, ini->section[i]->key, "version", INIVAL_TYPE_STR)
            conv_str(ctx, tests[z].version)

            getter(ini, ini->section[i]->key, "repository", INIVAL_TYPE_STR)
            conv_str(ctx, tests[z].repository)

            getter(ini, ini->section[i]->key, "script", INIVAL_TYPE_STR)
            conv_str_noexpand(ctx, tests[z].script)

            getter(ini, ini->section[i]->key, "build_recipe", INIVAL_TYPE_STR);
            conv_str(ctx, tests[z].build_recipe)

            z++;
        }
    }

    char env_name[NAME_MAX];
    char env_date[NAME_MAX];
    ctx->meta.python_compact = to_short_version(ctx->meta.python);

    if (!strcasecmp(ctx->meta.mission, "hst") && ctx->meta.final) {
        memset(env_date, 0, sizeof(env_date));
        strftime(env_date, sizeof(env_date) - 1, "%Y%m%d", ctx->info.time_info);
        snprintf(env_name, sizeof(env_name) - 1, "%s_%s_%s_py%s_final",
                 ctx->meta.name, env_date, ctx->system.platform[DELIVERY_PLATFORM_RELEASE], ctx->meta.python_compact);
    } else if (!strcasecmp(ctx->meta.mission, "hst")) {
        snprintf(env_name, sizeof(env_name) - 1, "%s_%s_%s_py%s_rc%d",
                 ctx->meta.name, ctx->meta.codename, ctx->system.platform[DELIVERY_PLATFORM_RELEASE], ctx->meta.python_compact, ctx->meta.rc);
    } else if (!strcasecmp(ctx->meta.mission, "jwst") && ctx->meta.final) {
        snprintf(env_name, sizeof(env_name), "%s_%s_%s_py%s_final",
                 ctx->meta.name, ctx->meta.version, ctx->system.platform[DELIVERY_PLATFORM_RELEASE], ctx->meta.python_compact);
    } else if (!strcasecmp(ctx->meta.mission, "jwst")) {
        snprintf(env_name, sizeof(env_name) - 1, "%s_%s_py%s_rc%d",
                 ctx->meta.name, ctx->meta.version, ctx->meta.python_compact, ctx->meta.rc);
    }

    if (strlen(env_name)) {
        ctx->info.release_name = strdup(env_name);
    }

    return 0;
}

void delivery_meta_show(struct Delivery *ctx) {
    printf("====DELIVERY====\n");
    printf("%-20s %-10s\n", "Target Python:", ctx->meta.python);
    printf("%-20s %-10s\n", "Name:", ctx->meta.name);
    printf("%-20s %-10s\n", "Mission:", ctx->meta.mission);
    if (ctx->meta.codename) {
        printf("%-20s %-10s\n", "Codename:", ctx->meta.codename);
    }
    if (ctx->meta.version) {
        printf("%-20s %-10s\n", "Version", ctx->meta.version);
    }
    if (!ctx->meta.final) {
        printf("%-20s %-10d\n", "RC Level:", ctx->meta.rc);
    }
    printf("%-20s %-10s\n", "Final Release:", ctx->meta.final ? "Yes" : "No");
    printf("%-20s %-10s\n", "Based On:", ctx->meta.based_on ? ctx->meta.based_on : "New");
}

void delivery_conda_show(struct Delivery *ctx) {
    printf("====CONDA====\n");
    printf("%-20s %-10s\n", "Installer:", ctx->conda.installer_baseurl);

    puts("Native Packages:");
    for (size_t i = 0; i < strlist_count(ctx->conda.conda_packages); i++) {
        char *token = strlist_item(ctx->conda.conda_packages, i);
        if (isempty(token) || isblank(*token) || startswith(token, "-")) {
            continue;
        }
        printf("%21s%s\n", "", token);
    }

    puts("Python Packages:");
    for (size_t i = 0; i < strlist_count(ctx->conda.pip_packages); i++) {
        char *token = strlist_item(ctx->conda.pip_packages, i);
        if (isempty(token) || isblank(*token) || startswith(token, "-")) {
            continue;
        }
        printf("%21s%s\n", "", token);
    }
}

void delivery_tests_show(struct Delivery *ctx) {
    printf("====TESTS====\n");
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        if (!ctx->tests[i].name) {
            continue;
        }
        printf("%-20s %-10s %s\n", ctx->tests[i].name,
               ctx->tests[i].version,
               ctx->tests[i].repository);
    }
}

int delivery_build_recipes(struct Delivery *ctx) {
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        char *recipe_dir = NULL;
        if (ctx->tests[i].build_recipe) { // build a conda recipe
            int recipe_type;
            int status;
            if (recipe_clone(ctx->storage.build_recipes_dir, ctx->tests[i].build_recipe, NULL, &recipe_dir)) {
                fprintf(stderr, "Encountered an issue while cloning recipe for: %s\n", ctx->tests[i].name);
                return -1;
            }
            recipe_type = recipe_get_type(recipe_dir);
            pushd(recipe_dir);
            {
                if (RECIPE_TYPE_ASTROCONDA == recipe_type) {
                    pushd(path_basename(ctx->tests[i].repository));
                } else if (RECIPE_TYPE_CONDA_FORGE == recipe_type) {
                    pushd("recipe");
                }

                char recipe_version[100];
                char recipe_buildno[100];
                char recipe_git_url[PATH_MAX];
                char recipe_git_rev[PATH_MAX];

                sprintf(recipe_version, "{%% set version = GIT_DESCRIBE_TAG ~ \".dev\" ~ GIT_DESCRIBE_NUMBER ~ \"+\" ~ GIT_DESCRIBE_HASH %%}");
                sprintf(recipe_git_url, "  git_url: %s", ctx->tests[i].repository);
                sprintf(recipe_git_rev, "  git_rev: %s", ctx->tests[i].version);
                sprintf(recipe_buildno, "  number: 0");

                //file_replace_text("meta.yaml", "{% set version = ", recipe_version);
                if (ctx->meta.final) {
                    sprintf(recipe_version, "{%% set version = \"%s\" %%}", ctx->tests[i].version);
                    // TODO: replace sha256 of tagged archive
                    // TODO: leave the recipe unchanged otherwise. in theory this should produce the same conda package hash as conda forge.
                    // For now, remove the sha256 requirement
                    file_replace_text("meta.yaml", "  sha256:", "\n");
                } else {
                    file_replace_text("meta.yaml", "{% set version = ", recipe_version);
                    file_replace_text("meta.yaml", "  url:", recipe_git_url);
                    file_replace_text("meta.yaml", "  sha256:", recipe_git_rev);
                    file_replace_text("meta.yaml", "  number:", recipe_buildno);
                }

                char command[PATH_MAX];
                sprintf(command, "build --python=%s .", ctx->meta.python);
                status = conda_exec(command);
                if (status) {
                    return -1;
                }

                if (RECIPE_TYPE_GENERIC != recipe_type) {
                    popd();
                }
                popd();
            }
        }
        if (recipe_dir) {
            free(recipe_dir);
        }
    }
    return 0;
}

struct StrList *delivery_build_wheels(struct Delivery *ctx) {
    struct StrList *result = NULL;
    struct Process proc;
    memset(&proc, 0, sizeof(proc));

    result = strlist_init();
    if (!result) {
        perror("unable to allocate memory for string list");
        result = NULL;
        return NULL;
    }

    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        if (!ctx->tests[i].build_recipe && ctx->tests[i].repository) { // build from source
            char srcdir[PATH_MAX];
            char wheeldir[PATH_MAX];
            memset(srcdir, 0, sizeof(srcdir));
            memset(wheeldir, 0, sizeof(wheeldir));

            sprintf(srcdir, "%s/%s", ctx->storage.build_sources_dir, ctx->tests[i].name);
            git_clone(&proc, ctx->tests[i].repository, srcdir, ctx->tests[i].version);
            pushd(srcdir);
            {
                if (python_exec("-m build -w ")) {
                    fprintf(stderr, "failed to generate wheel package for %s-%s\n", ctx->tests[i].name, ctx->tests[i].version);
                    strlist_free(result);
                    result = NULL;
                    return NULL;
                } else {
                    DIR *dp;
                    struct dirent *rec;
                    dp = opendir("dist");
                    if (!dp) {
                        fprintf(stderr, "wheel artifact directory does not exist: %s\n", ctx->storage.wheel_artifact_dir);
                        strlist_free(result);
                        return NULL;
                    }

                    while ((rec = readdir(dp)) != NULL) {
                        if (strstr(rec->d_name, ctx->tests[i].name)) {
                            strlist_append(result, rec->d_name);
                        }
                    }

                }
                popd();
            }
        }
    }
    return result;
}

static char *requirement_from_test(struct Delivery *ctx, const char *name) {
    static char result[PATH_MAX];
    memset(result, 0, sizeof(result));
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        if (!strcmp(ctx->tests[i].name, name)) {
            sprintf(result, "git+%s@%s",
                    ctx->tests[i].repository,
                    ctx->tests[i].version);
            break;
        }
    }
    if (!strlen(result)) {
        return NULL;
    }
    return result;
}

int delivery_install_packages(struct Delivery *ctx, char *conda_install_dir, char *env_name, int type, struct StrList **manifest) {
    char cmd[PATH_MAX];
    char pkgs[OMC_BUFSIZ];
    char *env_current = getenv("CONDA_DEFAULT_ENV");

    if (env_current) {
        // The requested environment is not the current environment
        if (strcmp(env_current, env_name) != 0) {
            // Activate the requested environment
            printf("Activating: %s\n", env_name);
            conda_activate(conda_install_dir, env_name);
            runtime_replace(&ctx->runtime.environ, __environ);
        }
    }

    memset(cmd, 0, sizeof(cmd));
    memset(pkgs, 0, sizeof(pkgs));
    strcat(cmd, "install");

    typedef int (*Runner)(const char *);
    Runner runner = NULL;
    if (INSTALL_PKG_CONDA & type) {
        runner = conda_exec;
    } else if (INSTALL_PKG_PIP & type) {
        runner = pip_exec;
    }

    if (INSTALL_PKG_CONDA_DEFERRED & type) {
        strcat(cmd, " --use-local");
    } else if (INSTALL_PKG_PIP_DEFERRED & type) {
        strcat(cmd, " --upgrade");
    }

    for (size_t x = 0; manifest[x] != NULL; x++) {
        char *name = NULL;
        for (size_t p = 0; p < strlist_count(manifest[x]); p++) {
            name = strlist_item(manifest[x], p);
            strip(name);
            if (!strlen(name)) {
                continue;
            }
            if (INSTALL_PKG_PIP_DEFERRED & type) {
                //DIR *dp;
                //struct dirent *rec;

                //dp = opendir(ctx->storage.wheel_artifact_dir);
                //if (!dp) {
                //    perror(ctx->storage.wheel_artifact_dir);
                //    exit(1);
                //}

                //char pyver_compact[100];
                //sprintf(pyver_compact, "-cp%s", ctx->meta.python);
                //strchrdel(pyver_compact, ".");
                //while ((rec = readdir(dp)) != NULL) {
                //    struct Wheel *wheelfile = NULL;
                //    if (!strcmp(rec->d_name, ".") || !strcmp(rec->d_name, "..")) {
                //        continue;
                //    }
                //    if (DT_DIR == rec->d_type && startswith(rec->d_name, name)) {
                //        wheelfile = get_wheel_file(ctx->storage.wheel_artifact_dir, name, (char *[]) {pyver_compact, NULL});
                //        if (wheelfile) {
                //            sprintf(cmd + strlen(cmd), " %s/%s", wheelfile->path_name, wheelfile->file_name);
                //            free(wheelfile);
                //            break;
                //        }
                //    }
                //}
                //closedir(dp);
                char *requirement = requirement_from_test(ctx, name);
                if (requirement) {
                    sprintf(cmd + strlen(cmd), " '%s'", requirement);
                }

            } else {
                if (startswith(name, "--") || startswith(name, "-")) {
                    sprintf(cmd + strlen(cmd), " %s", name);
                } else {
                    sprintf(cmd + strlen(cmd), " '%s'", name);
                }
            }
        }
        int status = runner(cmd);
        if (status) {
            return status;
        }
    }
    return 0;
}

void delivery_get_installer_url(struct Delivery *delivery, char *result) {
    if (delivery->conda.installer_version) {
        // Use version specified by configuration file
        sprintf(result, "%s/%s-%s-%s-%s.sh", delivery->conda.installer_baseurl,
                delivery->conda.installer_name,
                delivery->conda.installer_version,
                delivery->conda.installer_platform,
                delivery->conda.installer_arch);
    } else {
        // Use latest installer
        sprintf(result, "%s/%s-%s-%s.sh", delivery->conda.installer_baseurl,
                delivery->conda.installer_name,
                delivery->conda.installer_platform,
                delivery->conda.installer_arch);
    }

}

int delivery_get_installer(char *installer_url) {
    char *script = path_basename(installer_url);
    if (access(script, F_OK)) {
        // Script doesn't exist
        if (download(installer_url, script)) {
            // download failed
            return -1;
        }
    } else {
        msg(OMC_MSG_RESTRICT | OMC_MSG_L3, "Skipped, installer already exists\n", script);
    }
    return 0;
}

int delivery_copy_conda_artifacts(struct Delivery *ctx) {
    char cmd[PATH_MAX];
    char conda_build_dir[PATH_MAX];
    char subdir[PATH_MAX];
    memset(cmd, 0, sizeof(cmd));
    memset(conda_build_dir, 0, sizeof(conda_build_dir));
    memset(subdir, 0, sizeof(subdir));

    sprintf(conda_build_dir, "%s/%s", ctx->storage.conda_install_prefix, "conda-bld");
    // One must run conda build at least once to create the "conda-bld" directory.
    // When this directory is missing there can be no build artifacts.
    if (access(conda_build_dir, F_OK) < 0) {
        msg(OMC_MSG_RESTRICT | OMC_MSG_WARN | OMC_MSG_L3,
            "Skipped: 'conda build' has never been executed.\n");
        return 0;
    }

    snprintf(cmd, sizeof(cmd) - 1, "rsync -avi --progress %s/%s %s",
             conda_build_dir,
             ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR],
             ctx->storage.conda_artifact_dir);

    return system(cmd);
}

int delivery_copy_wheel_artifacts(struct Delivery *ctx) {
    char cmd[PATH_MAX];
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd) - 1, "rsync -avi --progress %s/*/dist/*.whl %s",
             ctx->storage.build_sources_dir,
             ctx->storage.wheel_artifact_dir);
    return system(cmd);
}

int delivery_index_wheel_artifacts(struct Delivery *ctx) {
    struct dirent *rec;
    DIR *dp;
    dp = opendir(ctx->storage.wheel_artifact_dir);
    if (!dp) {
        return -1;
    }

    while ((rec = readdir(dp)) != NULL) {
        // skip directories
        if (DT_DIR == rec->d_type || !endswith(rec->d_name, ".whl")) {
            continue;
        }
        char name[NAME_MAX];
        strcpy(name, rec->d_name);
        char **parts = split(name, "-", 1);
        strcpy(name, parts[0]);
        split_free(parts);

        tolower_s(name);
        char path_dest[PATH_MAX];
        sprintf(path_dest, "%s/%s/", ctx->storage.wheel_artifact_dir, name);
        mkdir(path_dest, 0755);
        sprintf(path_dest + strlen(path_dest), "%s", rec->d_name);

        char path_src[PATH_MAX];
        sprintf(path_src, "%s/%s", ctx->storage.wheel_artifact_dir, rec->d_name);
        rename(path_src, path_dest);
    }
    return 0;
}

void delivery_install_conda(char *install_script, char *conda_install_dir) {
    struct Process proc;
    memset(&proc, 0, sizeof(proc));

    if (!access(conda_install_dir, F_OK)) {
        // directory exists so remove it
        if (rmtree(conda_install_dir)) {
            perror("unable to remove previous installation");
            exit(1);
        }
    }

    // Proceed with the installation
    // -b = batch mode (non-interactive)
    if (shell_safe(&proc, (char *[]) {find_program("bash"), install_script, "-b", "-p", conda_install_dir, NULL})) {
        fprintf(stderr, "conda installation failed\n");
        exit(1);
    }
}

void delivery_conda_enable(struct Delivery *ctx, char *conda_install_dir) {
    if (conda_activate(conda_install_dir, "base")) {
        fprintf(stderr, "conda activation failed\n");
        exit(1);
    }

    // Setting the CONDARC environment variable appears to be the only consistent
    // way to make sure the file is used. Not setting this variable leads to strange
    // behavior, especially if a conda environment is already active when OMC is loaded.
    char rcpath[PATH_MAX];
    sprintf(rcpath, "%s/%s", conda_install_dir, ".condarc");
    setenv("CONDARC", rcpath, 1);
    if (runtime_replace(&ctx->runtime.environ, __environ)) {
        perror("unable to replace runtime environment after activating conda");
        exit(1);
    }

    conda_setup_headless();
}
void delivery_defer_packages(struct Delivery *ctx, int type) {
    struct StrList *dataptr = NULL;
    struct StrList *deferred = NULL;
    char *name = NULL;
    char cmd[PATH_MAX];

    memset(cmd, 0, sizeof(cmd));

    char mode[10];
    if (DEFER_CONDA == type) {
        dataptr = ctx->conda.conda_packages;
        deferred = ctx->conda.conda_packages_defer;
        strcpy(mode, "conda");
    } else if (DEFER_PIP == type) {
        dataptr = ctx->conda.pip_packages;
        deferred = ctx->conda.pip_packages_defer;
        strcpy(mode, "pip");
    }
    msg(OMC_MSG_L2, "Filtering %s packages by test definition...\n", mode);

    struct StrList *filtered = NULL;
    filtered = strlist_init();
    for (size_t i = 0, z = 0; i < strlist_count(dataptr); i++) {
        int ignore_pkg = 0;

        name = strlist_item(dataptr, i);
        if (!strlen(name) || isblank(*name) || isspace(*name)) {
            // no data
            continue;
        }
        msg(OMC_MSG_L3, "package '%s': ", name);

        // Compile a list of packages that are *also* to be tested.
        for (size_t x = 0; x < sizeof(ctx->tests) / sizeof(ctx->tests[0]); x++) {
            if (ctx->tests[x].name) {
                if (startswith(ctx->tests[x].name, name)) {
                    ignore_pkg = 1;
                    z++;
                    break;
                }
            }
        }

        if (ignore_pkg) {
            printf("BUILD FOR HOST\n");
            strlist_append(deferred, name);
        } else {
            printf("USE EXISTING\n");
            strlist_append(filtered, name);
        }
    }

    if (!strlist_count(deferred)) {
        msg(OMC_MSG_WARN | OMC_MSG_L2, "No %s packages were filtered by test definitions\n", mode);
    } else {
        if (DEFER_CONDA == type) {
            strlist_free(ctx->conda.conda_packages);
            ctx->conda.conda_packages = strlist_copy(filtered);
        } else if (DEFER_PIP == type) {
            strlist_free(ctx->conda.pip_packages);
            ctx->conda.pip_packages = strlist_copy(filtered);
        }
    }
    if (filtered) {
        strlist_free(filtered);
    }
}

const char *release_header = "# delivery_name: %s\n"
                     "# creation_time: %s\n"
                     "# conda_ident: %s\n"
                     "# conda_build_ident: %s\n";

char *delivery_get_release_header(struct Delivery *ctx) {
    char output[OMC_BUFSIZ];
    char stamp[100];
    strftime(stamp, sizeof(stamp) - 1, "%c", ctx->info.time_info);
    sprintf(output, release_header,
            ctx->info.release_name,
            stamp,
            ctx->conda.tool_version,
            ctx->conda.tool_build_version);
    return strdup(output);
}

void delivery_rewrite_spec(struct Delivery *ctx, char *filename) {
    char *package_name = NULL;
    char output[PATH_MAX];
    char *header = NULL;
    char *tempfile = NULL;
    FILE *tp = NULL;

    header = delivery_get_release_header(ctx);
    if (!header) {
        msg(OMC_MSG_ERROR, "failed to generate release header string\n", filename);
        exit(1);
    }
    tempfile = xmkstemp(&tp);
    if (!tempfile || !tp) {
        msg(OMC_MSG_ERROR, "%s: unable to create temporary file\n", strerror(errno));
        exit(1);
    }
    fprintf(tp, "%s", header);

    // Read the original file
    char **contents = file_readlines(filename, 0, 0, NULL);
    if (!contents) {
        msg(OMC_MSG_ERROR, "%s: unable to read %s", filename);
        exit(1);
    }

    // Write temporary data
    for (size_t i = 0; contents[i] != NULL; i++) {
        if (startswith(contents[i], "prefix:")) {
            // quick hack: prefixes are written at the bottom of the file
            // so strip it off
            break;
        }
        fprintf(tp, "%s", contents[i]);
    }

    for (size_t i = 0; contents[i] != NULL; i++) {
        free(contents[i]);
    }
    free(contents);
    contents = NULL;
    free(header);
    header = NULL;
    fflush(tp);
    fclose(tp);

    // Replace the original file with our temporary data
    if (copy2(tempfile, filename, CT_PERM) < 0) {
        fprintf(stderr, "%s: could not rename '%s' to '%s'\n", strerror(errno), tempfile, filename);
        exit(1);
    }

    // Replace "local" channel with the staging URL
    sprintf(output, "  - %s", ctx->storage.conda_staging_url);
    file_replace_text(filename, "  - local", output);

    // Rewrite tested packages to point to tested code, at a defined verison
    for (size_t i = 0; i < strlist_count(ctx->conda.pip_packages_defer); i++) {
        package_name = strlist_item(ctx->conda.pip_packages_defer, i);
        char target[PATH_MAX];
        char replacement[PATH_MAX];
        //struct Wheel *wheelfile;

        memset(target, 0, sizeof(target));
        memset(replacement, 0, sizeof(replacement));
        sprintf(target, "    - %s", package_name);
        // TODO: I still want to use wheels for this but tagging semantics are getting in the way.
        // When someone pushes a lightweight tag setuptools_scm will not resolve the expected
        // refs unless the following is present in pyproject.toml, setup.cfg, or setup.py:
        //
        // git_describe_command = "git describe --tags" # at the bare minimum
        //

        //char abi[NAME_MAX];
        //strcpy(abi, ctx->meta.python);
        //strchrdel(abi, ".");

        //char source_dir[PATH_MAX];
        //sprintf(source_dir, "%s/%s", ctx->storage.build_sources_dir, package_name);
        //wheelfile = get_wheel_file(ctx->storage.wheel_artifact_dir, package_name, (char *[]) {git_describe(source_dir), abi, ctx->system.arch, NULL});
        //if (wheelfile) {
        //    sprintf(replacement, "    - %s/%s", ctx->storage.wheel_staging_url, wheelfile->file_name);
        //    file_replace_text(filename, target, replacement);
        //}
        // end of TODO

        char *requirement = requirement_from_test(ctx, package_name);
        if (requirement) {
            sprintf(replacement, "    - %s", requirement);
            file_replace_text(filename, target, replacement);
        } else {
            fprintf(stderr, "an error occurred while rewriting a release artifact: %s\n", filename);
            fprintf(stderr, "mapping a replacement value for package defined by '[test:%s]' failed: %s\n", package_name, package_name);
            fprintf(stderr, "target string in artifact was:\n%s\n", target);
            exit(1);
        }
    }
}

int delivery_index_conda_artifacts(struct Delivery *ctx) {
    return conda_index(ctx->storage.conda_artifact_dir);
}

void delivery_tests_run(struct Delivery *ctx) {
    struct Process proc;
    if (!ctx->tests[0].name) {
        msg(OMC_MSG_WARN | OMC_MSG_L2, "no tests are defined!\n");
    } else {
        for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
            if (!ctx->tests[i].name && !ctx->tests[i].repository && !ctx->tests[i].script) {
                // skip unused test records
                continue;
            }
            msg(OMC_MSG_L2, "%s %s\n", ctx->tests[i].name, ctx->tests[i].version);
            if (!ctx->tests[i].script || !strlen(ctx->tests[i].script)) {
                msg(OMC_MSG_WARN | OMC_MSG_L3, "Nothing to do. To fix, declare a 'script' in section: [test:%s]\n",
                    ctx->tests[i].name);
                continue;
            }

            char destdir[PATH_MAX];
            sprintf(destdir, "%s/%s", ctx->storage.build_sources_dir, path_basename(ctx->tests[i].repository));

            msg(OMC_MSG_L3, "Cloning %s\n", ctx->tests[i].repository);
            git_clone(&proc, ctx->tests[i].repository, destdir, ctx->tests[i].version);

            if (pushd(destdir)) {
                COE_CHECK_ABORT(!globals.continue_on_error, "Unable to enter repository directory\n");
            } else {
#if 1
                int status;
                char cmd[PATH_MAX];
                msg(OMC_MSG_L3, "Testing %s\n", ctx->tests[i].name);
                memset(&proc, 0, sizeof(proc));

                // enable trace mode before executing each test script
                memset(cmd, 0, sizeof(cmd));
                sprintf(cmd, "set -x ; %s", ctx->tests[i].script);
                status = shell2(&proc, cmd);
                if (status) {
                    COE_CHECK_ABORT(!globals.continue_on_error, "Test failure");
                }
                popd();
#else
                msg(OMC_MSG_WARNING | OMC_MSG_L3, "TESTING DISABLED BY CODE!\n");
#endif
            }
        }
    }
}

void delivery_gather_tool_versions(struct Delivery *ctx) {
    int status = 0;

    // Extract version from tool output
    ctx->conda.tool_version = shell_output("conda --version", &status);
    if (ctx->conda.tool_version)
        strip(ctx->conda.tool_version);

    ctx->conda.tool_build_version = shell_output("conda build --version", &status);
    if (ctx->conda.tool_build_version)
        strip(ctx->conda.tool_version);
}
