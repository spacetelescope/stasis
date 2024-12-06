//
// Created by jhunk on 11/15/24.
//

#include "core.h"
#include "helpers.h"

struct StrList *get_architectures(struct Delivery ctx[], const size_t nelem) {
    struct StrList *architectures = strlist_init();
    for (size_t i = 0; i < nelem; i++) {
        if (ctx[i].system.arch) {
            if (!strstr_array(architectures->data, ctx[i].system.arch)) {
                strlist_append(&architectures, ctx[i].system.arch);
            }
        }
    }
    return architectures;
}

struct StrList *get_platforms(struct Delivery ctx[], const size_t nelem) {
    struct StrList *platforms = strlist_init();
    for (size_t i = 0; i < nelem; i++) {
        if (ctx[i].system.platform) {
            if (!strstr_array(platforms->data, ctx[i].system.platform[DELIVERY_PLATFORM_RELEASE])) {
                strlist_append(&platforms, ctx[i].system.platform[DELIVERY_PLATFORM_RELEASE]);
            }
        }
    }
    return platforms;
}

int get_pandoc_version(size_t *result) {
    *result = 0;
    int state = 0;
    char *version_str = shell_output("pandoc --version", &state);
    if (state || !version_str) {
        // an error occurred
        return -1;
    }

    // Verify that we're looking at pandoc
    if (strlen(version_str) > 7 && !strncmp(version_str, "pandoc ", 7)) {
        // we have pandoc
        char *v_begin = &version_str[7];
        if (!v_begin) {
            SYSERROR("unexpected pandoc output: %s", version_str);
            return -1;
        }
        char *v_end = strchr(version_str, '\n');
        if (v_end) {
            *v_end = 0;
        }

        char **parts = split(v_begin, ".", 0);
        if (!parts) {
            SYSERROR("unable to split pandoc version string, '%s': %s", version_str, strerror(errno));
            return -1;
        }

        size_t parts_total;
        for (parts_total = 0; parts[parts_total] != NULL; parts_total++) {}

        // generate the version as an integer
        // note: pandoc version scheme never exceeds four elements (or bytes in this case)
        for (size_t i = 0; i < 4; i++) {
            unsigned char tmp = 0;
            if (i < parts_total) {
                // only process version elements we have. the rest will be zeros.
                tmp = strtoul(parts[i], NULL, 10);
            }
            // pack version element into result
            *result = *result << 8 | tmp;
        }
    } else {
        // invalid version string
        return 1;
    }

    return 0;
}

int pandoc_exec(const char *in_file, const char *out_file, const char *css_file, const char *title) {
    if (!find_program("pandoc")) {
        fprintf(stderr, "pandoc is not installed: unable to generate HTML indexes\n");
        return 0;
    }

    char pandoc_versioned_args[255] = {0};
    size_t pandoc_version = 0;
    if (!get_pandoc_version(&pandoc_version)) {
        // < 2.19
        if (pandoc_version < 0x02130000) {
            strcat(pandoc_versioned_args, "--self-contained ");
        } else {
            // >= 2.19
            strcat(pandoc_versioned_args, "--embed-resources ");
        }

        // >= 1.15.0.4
        if (pandoc_version >= 0x010f0004) {
            strcat(pandoc_versioned_args, "--standalone ");
        }

        // >= 1.10.0.1
        if (pandoc_version >= 0x010a0001) {
            strcat(pandoc_versioned_args, "-f gfm+autolink_bare_uris ");
        }

        // > 3.1.9
        if (pandoc_version > 0x03010900) {
            strcat(pandoc_versioned_args, "-f gfm+alerts ");
        }
    }

    // Converts a markdown file to html
    char cmd[STASIS_BUFSIZ] = {0};
    strcpy(cmd, "pandoc ");
    strcat(cmd, pandoc_versioned_args);
    if (css_file && strlen(css_file)) {
        strcat(cmd, "--css ");
        strcat(cmd, css_file);
    }
    strcat(cmd, " ");
    strcat(cmd, "--metadata title=\"");
    strcat(cmd, title);
    strcat(cmd, "\" ");
    strcat(cmd, "-o ");
    strcat(cmd, out_file);
    strcat(cmd, " ");
    strcat(cmd, in_file);

    if (globals.verbose) {
        puts(cmd);
    }

    // This might be negative when killed by a signal.
    return system(cmd);
}


int micromamba_configure(const struct Delivery *ctx, struct MicromambaInfo *m) {
    int status = 0;
    char *micromamba_prefix = NULL;
    if (asprintf(&micromamba_prefix, "%s/bin", ctx->storage.tools_dir) < 0) {
        return -1;
    }
    m->conda_prefix = globals.conda_install_prefix;
    m->micromamba_prefix = micromamba_prefix;

    const size_t pathvar_len = strlen(getenv("PATH")) + strlen(m->micromamba_prefix) + strlen(m->conda_prefix) + 3 + 4 + 1;
    // ^^^^^^^^^^^^^^^^^^
    // 3 = separators
    // 4 = chars (/bin)
    // 1 = nul terminator
    char *pathvar = calloc(pathvar_len, sizeof(*pathvar));
    if (!pathvar) {
        SYSERROR("%s", "Unable to allocate bytes for temporary path string");
        exit(1);
    }
    snprintf(pathvar, pathvar_len, "%s/bin:%s:%s", m->conda_prefix, m->micromamba_prefix, getenv("PATH"));
    setenv("PATH", pathvar, 1);
    guard_free(pathvar);

    status += micromamba(m, "config prepend --env channels conda-forge");
    if (!globals.verbose) {
        status += micromamba(m, "config set --env quiet true");
    }
    status += micromamba(m, "config set --env always_yes true");
    status += micromamba(m, "install conda-build pandoc");

    return status;
}

int get_latest_rc(struct Delivery ctx[], const size_t nelem) {
    int result = 0;
    for (size_t i = 0; i < nelem; i++) {
        if (ctx[i].meta.rc > result) {
            result = ctx[i].meta.rc;
        }
    }
    return result;
}

int sort_by_latest_rc(const void *a, const void *b) {
    const struct Delivery *aa = a;
    const struct Delivery *bb = b;
    if (aa->meta.rc > bb->meta.rc) {
        return -1;
    }
    if (aa->meta.rc < bb->meta.rc) {
        return 1;
    }
    return 0;
}

struct Delivery *get_latest_deliveries(struct Delivery ctx[], size_t nelem) {
    int latest = 0;
    size_t n = 0;

    struct Delivery *result = calloc(nelem + 1, sizeof(*result));
    if (!result) {
        fprintf(stderr, "Unable to allocate %zu bytes for result delivery array: %s\n", nelem * sizeof(*result), strerror(errno));
        return NULL;
    }

    latest = get_latest_rc(ctx, nelem);
    qsort(ctx, nelem, sizeof(*ctx), sort_by_latest_rc);
    for (size_t i = 0; i < nelem; i++) {
        if (ctx[i].meta.rc == latest) {
            result[n] = ctx[i];
            n++;
        }
    }
    return result;
}

int get_files(struct StrList **out, const char *path, const char *pattern, ...) {
    va_list args;
    va_start(args, pattern);
    char userpattern[PATH_MAX] = {0};
    vsprintf(userpattern, pattern, args);
    va_end(args);
    struct StrList *list = listdir(path);
    if (!list) {
        return -1;
    }

    if (!*out) {
        *out = strlist_init();
        if (!*out) {
            guard_strlist_free(&list);
            return -1;
        }
    }

    size_t no_match = 0;
    for (size_t i = 0; i < strlist_count(list); i++) {
        char *item = strlist_item(list, i);
        if (fnmatch(userpattern, item, 0)) {
            no_match++;
        } else {
            strlist_append(out, item);
        }
    }
    if (no_match >= strlist_count(list)) {
        fprintf(stderr, "no files matching the pattern: %s\n", userpattern);
        guard_strlist_free(&list);
        return -1;
    }
    guard_strlist_free(&list);
    return 0;
}

int load_metadata(struct Delivery *ctx, const char *filename) {
    char line[STASIS_NAME_MAX] = {0};

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return -1;
    }

    while (fgets(line, sizeof(line) - 1, fp) != NULL) {
        char **parts = split(line, " ", 1);
        const char *name = parts[0];
        char *value = parts[1];

        strip(value);
        if (!strcmp(name, "name")) {
            ctx->meta.name = strdup(value);
        } else if (!strcmp(name, "version")) {
            ctx->meta.version = strdup(value);
        } else if (!strcmp(name, "rc")) {
            ctx->meta.rc = (int) strtol(value, NULL, 10);
        } else if (!strcmp(name, "python")) {
            ctx->meta.python = strdup(value);
        } else if (!strcmp(name, "python_compact")) {
            ctx->meta.python_compact = strdup(value);
        } else if (!strcmp(name, "mission")) {
            ctx->meta.mission = strdup(value);
        } else if (!strcmp(name, "codename")) {
            ctx->meta.codename = strdup(value);
        } else if (!strcmp(name, "platform")) {
            ctx->system.platform = split(value, " ", 0);
        } else if (!strcmp(name, "arch")) {
            ctx->system.arch = strdup(value);
        } else if (!strcmp(name, "time")) {
            ctx->info.time_str_epoch = strdup(value);
        } else if (!strcmp(name, "release_fmt")) {
            ctx->rules.release_fmt = strdup(value);
        } else if (!strcmp(name, "release_name")) {
            ctx->info.release_name = strdup(value);
        } else if (!strcmp(name, "build_name_fmt")) {
            ctx->rules.build_name_fmt = strdup(value);
        } else if (!strcmp(name, "build_name")) {
            ctx->info.build_name = strdup(value);
        } else if (!strcmp(name, "build_number_fmt")) {
            ctx->rules.build_number_fmt = strdup(value);
        } else if (!strcmp(name, "build_number")) {
            ctx->info.build_number = strdup(value);
        } else if (!strcmp(name, "conda_installer_baseurl")) {
            ctx->conda.installer_baseurl = strdup(value);
        } else if (!strcmp(name, "conda_installer_name")) {
            ctx->conda.installer_name = strdup(value);
        } else if (!strcmp(name, "conda_installer_version")) {
            ctx->conda.installer_version = strdup(value);
        } else if (!strcmp(name, "conda_installer_platform")) {
            ctx->conda.installer_platform = strdup(value);
        } else if (!strcmp(name, "conda_installer_arch")) {
            ctx->conda.installer_arch = strdup(value);
        }
        GENERIC_ARRAY_FREE(parts);
    }
    fclose(fp);

    return 0;
}
