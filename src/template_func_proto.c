#include "template_func_proto.h"

int get_github_release_notes_tplfunc_entrypoint(void *frame, void *data_out) {
    int result;
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    char *api_token = getenv("STASIS_GH_TOKEN");
    if (!api_token) {
        api_token = getenv("GITHUB_TOKEN");
    }
    result = get_github_release_notes(api_token ? api_token : "anonymous",
                                      (const char *) f->argv[0].t_char_ptr,
                                      (const char *) f->argv[1].t_char_ptr,
                                      (const char *) f->argv[2].t_char_ptr,
                                      output);
    return result;
}

int get_github_release_notes_auto_tplfunc_entrypoint(void *frame, void *data_out) {
    int result = 0;
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    char *api_token = getenv("STASIS_GH_TOKEN");
    if (!api_token) {
        api_token = getenv("GITHUB_TOKEN");
    }

    const struct Delivery *ctx = (struct Delivery *) f->data_in;
    struct StrList *notes_list = strlist_init();
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(*ctx->tests); i++) {
        // Get test context
        const struct Test *test = &ctx->tests[i];
        if (test->name && test->version && test->repository) {
            char *repository = strdup(test->repository);
            char *match = strstr(repository, "spacetelescope/");
            // Cull repository URL
            if (match) {
                replace_text(repository, "https://github.com/", "", 0);
                if (endswith(repository, ".git")) {
                    replace_text(repository, ".git", "", 0);
                }
                // Record release notes for version relative to HEAD
                // Using HEAD, GitHub returns the previous tag
                char *note = NULL;
                char h1_title[NAME_MAX] = {0};
                sprintf(h1_title, "# %s", test->name);
                strlist_append(&notes_list, h1_title);
                result += get_github_release_notes(api_token ? api_token : "anonymous",
                                                   repository,
                                                   test->version,
                                                   "HEAD",
                                                   &note);
                if (note) {
                    strlist_append(&notes_list, note);
                    guard_free(note);
                }
                guard_free(repository);
            }
        }
    }
    // Return all notes as a single string
    if (strlist_count(notes_list)) {
        *output = join(notes_list->data, "\n\n");
    }
    guard_strlist_free(&notes_list);

    return result;
}

int get_junitxml_file_entrypoint(void *frame, void *data_out) {
    int result = 0;
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    const struct Delivery *ctx = (const struct Delivery *) f->data_in;

    char cwd[PATH_MAX] = {0};
    getcwd(cwd, PATH_MAX - 1);
    char nametmp[PATH_MAX] = {0};
    strcpy(nametmp, cwd);
    char *name = path_basename(nametmp);

    *output = calloc(PATH_MAX, sizeof(**output));
    if (!*output) {
        SYSERROR("failed to allocate output string: %s", strerror(errno));
        return -1;
    }
    sprintf(*output, "%s/results-%s-%s.xml", ctx->storage.results_dir, name, ctx->info.release_name);

    return result;
}

int get_basetemp_dir_entrypoint(void *frame, void *data_out) {
    int result = 0;
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    const struct Delivery *ctx = (const struct Delivery *) f->data_in;

    char cwd[PATH_MAX] = {0};
    getcwd(cwd, PATH_MAX - 1);
    char nametmp[PATH_MAX] = {0};
    strcpy(nametmp, cwd);
    char *name = path_basename(nametmp);

    *output = calloc(PATH_MAX, sizeof(**output));
    if (!*output) {
        SYSERROR("failed to allocate output string: %s", strerror(errno));
        return -1;
    }
    sprintf(*output, "%s/truth-%s-%s", ctx->storage.tmpdir, name, ctx->info.release_name);

    return result;
}

int tox_run_entrypoint(void *frame, void *data_out) {
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    const struct Delivery *ctx = (const struct Delivery *) f->data_in;

    // Apply workaround for tox positional arguments
    char *toxconf = NULL;
    if (!access("tox.ini", F_OK)) {
        if (!fix_tox_conf("tox.ini", &toxconf)) {
            msg(STASIS_MSG_L3, "Fixing tox positional arguments\n");
            *output = calloc(STASIS_BUFSIZ, sizeof(**output));
            if (!*output) {
                return -1;
            }
            char *basetemp_path = NULL;
            if (get_basetemp_dir_entrypoint(f, &basetemp_path)) {
                return -2;
            }
            char *jxml_path = NULL;
            if (get_junitxml_file_entrypoint(f, &jxml_path)) {
                return -3;
            }
            const char *tox_target = f->argv[0].t_char_ptr;
            const char *pytest_args = f->argv[1].t_char_ptr;
            if (isempty(toxconf) || !strcmp(toxconf, "/")) {
                SYSERROR("Unsafe toxconf path: '%s'", toxconf);
                return -4;
            }
            snprintf(*output, STASIS_BUFSIZ - 1, "\npip install tox && (tox -e py%s%s -c %s --root . -- --basetemp=\"%s\" --junitxml=\"%s\" %s ; rm -f '%s')\n", ctx->meta.python_compact, tox_target, toxconf, basetemp_path, jxml_path, pytest_args ? pytest_args : "", toxconf);

            guard_free(jxml_path);
            guard_free(basetemp_path);
        }
    }
    return 0;
}