//
// Created by jhunk on 10/5/23.
//

#include "deliverable.h"
#include "str.h"
#include "strlist.h"
#include "wheel.h"

#define getter(XINI, SECTION_NAME, KEY, TYPE) \
    { \
        if (ini_getval(XINI, SECTION_NAME, KEY, TYPE, &val)) { \
            fprintf(stderr, "%s:%s not defined\n", SECTION_NAME, KEY); \
        } \
    }

#define conv_int(X, DEST) X->DEST = val.as_int;
#define conv_str(X, DEST) X->DEST = runtime_expand_var(NULL, val.as_char_p);
#define conv_str_noexpand(X, DEST) if (val.as_char_p) X->DEST = strdup(val.as_char_p);
#define conv_strlist(X, DEST, TOK) { \
    char *rtevnop = runtime_expand_var(NULL, val.as_char_p); \
    if (!X->DEST)               \
        X->DEST = strlist_init(); \
    strlist_append_tokenize(X->DEST, rtevnop, TOK);    \
    free(rtevnop);\
}
#define conv_bool(X, DEST) X->DEST = val.as_bool;

void delivery_init_dirs(struct Delivery *ctx) {
    mkdir("build", 0755);
    mkdir("build/recipes", 0755);
    mkdir("build/sources", 0755);
    mkdir("build/testing", 0755);
    ctx->storage.build_dir = realpath("build", NULL);
    ctx->storage.build_recipes_dir = realpath("build/recipes", NULL);
    ctx->storage.build_sources_dir = realpath("build/sources", NULL);
    ctx->storage.build_testing_dir = realpath("build/testing", NULL);

    mkdir("output", 0755);
    mkdir("output/omc", 0755);
    mkdir("output/packages", 0755);
    mkdir("output/packages/conda", 0755);
    mkdir("output/packages/wheels", 0755);
    ctx->storage.delivery_dir = realpath("output/omc", NULL);
    ctx->storage.conda_artifact_dir = realpath("output/packages/conda", NULL);
    ctx->storage.wheel_artifact_dir = realpath("output/packages/wheels", NULL);

    mkdir(CONDA_INSTALL_PREFIX, 0755);
    ctx->storage.conda_install_prefix = realpath(CONDA_INSTALL_PREFIX, NULL);
}

int delivery_init(struct Delivery *ctx, struct INIFILE *ini, struct INIFILE *cfg) {
    RuntimeEnv *rt;
    struct INIData *rtdata;
    union INIVal val;

    if (cfg) {
        getter(cfg, "default", "conda_staging_dir", INIVAL_TYPE_STR);
        conv_str(ctx, storage.conda_staging_dir);
        getter(cfg, "default", "conda_staging_url", INIVAL_TYPE_STR);
        conv_str(ctx, storage.conda_staging_url);
        getter(cfg, "default", "wheel_staging_dir", INIVAL_TYPE_STR);
        conv_str(ctx, storage.wheel_staging_dir);
        getter(cfg, "default", "wheel_staging_url", INIVAL_TYPE_STR);
        conv_str(ctx, storage.wheel_staging_url);
    }
    delivery_init_dirs(ctx);

    // Populate runtime variables first they may be interpreted by other
    // keys in the configuration
    rt = runtime_copy(__environ);
    while ((rtdata = ini_getall(ini, "runtime")) != NULL) {
        char rec[BUFSIZ];
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

    getter(ini, "meta", "name", INIVAL_TYPE_STR)
    conv_str(ctx, meta.name)

    getter(ini, "meta", "rc", INIVAL_TYPE_INT)
    conv_int(ctx, meta.rc)

    getter(ini, "meta", "final", INIVAL_TYPE_BOOL)
    conv_bool(ctx, meta.final)

    getter(ini, "meta", "continue_on_error", INIVAL_TYPE_BOOL)
    conv_bool(ctx, meta.continue_on_error)

    getter(ini, "meta", "based_on", INIVAL_TYPE_STR)
    conv_str(ctx, meta.based_on)

    getter(ini, "meta", "python", INIVAL_TYPE_STR)
    conv_str(ctx, meta.python)

    getter(ini, "conda", "installer_baseurl", INIVAL_TYPE_STR)
    conv_str(ctx, conda.installer_baseurl)

    getter(ini, "conda", "installer_name", INIVAL_TYPE_STR)
    conv_str(ctx, conda.installer_name)

    getter(ini, "conda", "installer_version", INIVAL_TYPE_STR)
    conv_str(ctx, conda.installer_version)

    getter(ini, "conda", "installer_platform", INIVAL_TYPE_STR)
    conv_str(ctx, conda.installer_platform)

    getter(ini, "conda", "installer_arch", INIVAL_TYPE_STR)
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
    char data[BUFSIZ];
    char *datap = data;

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

    puts("PyPi Packages:");
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
    char *recipe_dir = NULL;
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
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
                    fprintf(stderr, "failed to build deployment artifact: %s\n", ctx->tests[i].build_recipe);
                    msg(OMC_MSG_WARN | OMC_MSG_L1, "ENTERING DEBUG SHELL\n");
                    system("bash --noprofile --norc");
                    exit(1);
                    if (!ctx->meta.continue_on_error) {
                        return -1;
                    }
                }

                if (RECIPE_TYPE_GENERIC != recipe_type) {
                    popd();
                }
                popd();
            }
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
                    if (!ctx->meta.continue_on_error) {
                        strlist_free(result);
                        result = NULL;
                        return NULL;
                    }
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

void delivery_install_packages(struct Delivery *ctx, char *conda_install_dir, char *env_name, int type, struct StrList **manifest) {
    char cmd[PATH_MAX];
    char pkgs[BUFSIZ];
    char *env_current = getenv("CONDA_DEFAULT_ENV");

    if (env_current) {
        // The requested environment is not the current environment
        if (strcmp(env_current, env_name) != 0) {
            // Activate the requested environment
            printf("Activating: %s\n", env_name);
            conda_activate(conda_install_dir, env_name);
            //runtime_replace(&ctx->runtime.environ, __environ);
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
        if (runner(cmd)) {
            fprintf(stderr, "failed to install package: %s\n", name);
            exit(1);
        }
    }
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

void delivery_get_installer(char *installer_url) {
    if (access(path_basename(installer_url), F_OK)) {
        if (download(installer_url, path_basename(installer_url))) {
            fprintf(stderr, "download failed: %s\n", installer_url);
            exit(1);
        }
    }
}

int delivery_copy_conda_artifacts(struct Delivery *ctx) {
    char cmd[PATH_MAX];
    char conda_build_dir[PATH_MAX];
    char subdir[PATH_MAX];
    memset(cmd, 0, sizeof(cmd));
    memset(conda_build_dir, 0, sizeof(conda_build_dir));
    memset(subdir, 0, sizeof(subdir));

    sprintf(conda_build_dir, "%s/%s", ctx->storage.conda_install_prefix, "conda-bld");
    if (access(conda_build_dir, F_OK) < 0) {
        // Conda build was never executed
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

void delivery_rewrite_spec(struct Delivery *ctx, char *filename) {
    char *package_name = NULL;
    char output[PATH_MAX];

    sprintf(output, "  - %s", ctx->storage.conda_staging_url);
    file_replace_text(filename, "  - local", output);
    for (size_t i = 0; i < strlist_count(ctx->conda.pip_packages_defer); i++) {
        package_name = strlist_item(ctx->conda.pip_packages_defer, i);
        char target[PATH_MAX];
        char replacement[PATH_MAX];
        struct Wheel *wheelfile;

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
                // unused entry
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

            if (pushd(destdir) && !ctx->meta.continue_on_error) {
                fprintf(stderr, "unable to enter repository directory\n");
                exit(1);
            } else {
#if 1
                msg(OMC_MSG_L3, "Running\n");
                memset(&proc, 0, sizeof(proc));
                if (shell2(&proc, ctx->tests[i].script) && !ctx->meta.continue_on_error) {
                    fprintf(stderr, "continue on error is not enabled. aborting.\n");
                    exit(1);
                }
                popd();
#else
                msg(OMC_MSG_WARNING | OMC_MSG_L3, "TESTING DISABLED BY CODE!\n");
#endif
            }
        }
    }


}
