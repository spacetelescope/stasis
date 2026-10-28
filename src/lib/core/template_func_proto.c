#include "template_func_proto.h"
#include "delivery.h"
#include "github.h"
#include "utils.h"

int get_github_release_notes_tplfunc_entrypoint(struct tplfunc_frame *frame, void *data_out) {
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    char *api_token = getenv("STASIS_GH_TOKEN");
    if (!api_token) {
        api_token = getenv("GITHUB_TOKEN");
    }
    int result = get_github_release_notes(api_token ? api_token : "anonymous",
                                          (const char *) f->argv[0].t_char_ptr,
                                          (const char *) f->argv[1].t_char_ptr,
                                          (const char *) f->argv[2].t_char_ptr,
                                          output);
    return result;
}

int get_github_release_notes_auto_tplfunc_entrypoint(struct tplfunc_frame *frame, void *data_out) {
    int result = 0;
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    char *api_token = getenv("STASIS_GH_TOKEN");
    if (!api_token) {
        api_token = getenv("GITHUB_TOKEN");
    }

    const struct Delivery *ctx = (struct Delivery *) f->data_in;
    struct StrList *notes_list = strlist_init();
    for (size_t i = 0; i < ctx->tests->num_used; i++) {
        // Get test context
        const struct Test *test = ctx->tests->test[i];
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
                snprintf(h1_title, sizeof(h1_title), "# %s", test->name);
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
            }
            guard_free(repository);
        }
    }
    // Return all notes as a single string
    if (strlist_count(notes_list)) {
        *output = join(notes_list->data, "\n\n");
    }
    guard_strlist_free(&notes_list);

    return result;
}

static int sort_test_by_name(const void *aa, const void *bb) {
    const struct Test *a = aa;
    if (!a) {
        return -1;
    }
    const struct Test *b = bb;
    if (!b) {
        return 1;
    }
    return strcmp(a->name, b->name);
}

int get_git_release_notes_auto_tplfunc_entrypoint(struct tplfunc_frame *frame, void *data_out) {
    int result = 0;
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    const struct Delivery *ctx = (struct Delivery *) f->data_in;
    struct StrList *notes_list = strlist_init();

    qsort(ctx->tests->test, ctx->tests->num_used, sizeof(struct Test *), sort_test_by_name);

    for (size_t i = 0; i < ctx->tests->num_used; i++) {
        // Get test context
        struct Test *test = ctx->tests->test[i];
        if (test->name && test->version && test->repository) {
            // Record release notes for version relative to HEAD
            // Using HEAD, GitHub returns the previous tag
            char *note = NULL;
            char h1_title[NAME_MAX] = {0};
            snprintf(h1_title, sizeof(h1_title), "# %s", test->name);
            strlist_append(&notes_list, h1_title);

            char *repository_url = strdup(test->repository);
            if (!repository_url) {
                SYSERROR("unable to allocate repository url: %s", strerror(errno));
                guard_strlist_free(&notes_list);
            }
            replace_text(repository_url, ".git", "", 0);
            char *repository_basename = path_basename(repository_url);

            char repository_path[PATH_MAX] = {0};
            snprintf(repository_path, sizeof(repository_path),"%s/%s", ctx->storage.build_sources_dir, repository_basename);

            // is this a git-describe long format string?
            int use_head = 0;
            if (num_chars(test->version, '-') > 1) {
                const char *hash = strrchr(test->version, 'g') + 1;
                const char *post_commit_s = strrchr(hash - 2, '-') + 1;
                const size_t post_commit_num = strtoul(post_commit_s, NULL, 0);
                if (post_commit_num) {
                    use_head = 1;
                }
            }

            // is this a setuptool_scm style long format string?
            if (strstr(test->version, ".dev") && strrchr(test->version, '+')) {
                const char *post_commit_s = strstr(test->version, ".dev") + 4;
                const size_t post_commit_num = strtoul(post_commit_s, NULL, 0);
                if (post_commit_num) {
                    use_head = 1;
                }
            }

            if (!test->repository_info_tag) {
                struct Process proc;
                if (git_clone(&proc, repository_url, repository_path, test->version)) {
                    SYSERROR("clone failed: %s", repository_url);
                    guard_free(repository_url);
                    continue;
                }
            }
            if (!test->repository_info_tag) {
                const char *tmp = git_describe(repository_path);
                test->repository_info_tag = tmp ? strdup(tmp) : NULL;
            }
            if (!test->repository_info_ref) {
                const char *tmp = git_rev_parse(repository_path, test->repository_info_tag);
                test->repository_info_ref = tmp ? strdup(tmp) : NULL;
            }
            guard_free(repository_url);

            char *last_tag = get_git_tag(repository_path, test->repository_info_tag, use_head ? 0 : 1);
            if (last_tag) {
                result += get_git_release_notes(
                    repository_path,
                    last_tag,
                    use_head ? "HEAD" : test->repository_info_tag,
                    &note);
                guard_free(last_tag);
            }
            if (note) {
                strlist_append(&notes_list, note);
                guard_free(note);
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

int get_junitxml_file_entrypoint(struct tplfunc_frame *frame, void *data_out) {
    int result = 0;
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    const struct Delivery *ctx = (const struct Delivery *) f->data_in;

    char cwd[PATH_MAX] = {0};
    if (!getcwd(cwd, PATH_MAX - 1)) {
        SYSERROR("unable to determine current working directory: %s", strerror(errno));
        return -1;
    }
    char nametmp[PATH_MAX] = {0};
    safe_strncpy(nametmp, cwd, sizeof(nametmp));

    char *name = path_basename(nametmp);

    *output = calloc(PATH_MAX, sizeof(**output));
    if (!*output) {
        SYSERROR("failed to allocate output string: %s", strerror(errno));
        return -1;
    }
    snprintf(*output, PATH_MAX, "%s/results-%s-%s.xml",
        ctx->storage.results_dir ? ctx->storage.results_dir : "",
        name, ctx->info.release_name ? ctx->info.release_name : "");

    return result;
}

int get_basetemp_dir_entrypoint(struct tplfunc_frame *frame, void *data_out) {
    int result = 0;
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    const struct Delivery *ctx = (const struct Delivery *) f->data_in;

    char cwd[PATH_MAX] = {0};
    if (!getcwd(cwd, PATH_MAX - 1)) {
        SYSERROR("unable to determine current working directory: %s", strerror(errno));
        return -1;
    }
    char nametmp[PATH_MAX] = {0};
    safe_strncpy(nametmp, cwd, sizeof(nametmp));
    char *name = path_basename(nametmp);

    *output = calloc(PATH_MAX, sizeof(**output));
    if (!*output) {
        SYSERROR("failed to allocate output string: %s", strerror(errno));
        return -1;
    }
    snprintf(*output, PATH_MAX, "%s/truth-%s-%s",
        ctx->storage.tmpdir ? ctx->storage.tmpdir : "",
        name,
        ctx->info.release_name ? ctx->info.release_name : "");

    return result;
}

int tox_run_entrypoint(struct tplfunc_frame *frame, void *data_out) {
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    const struct Delivery *ctx = (const struct Delivery *) f->data_in;
    struct tpl_pool **tpl = (struct tpl_pool **) ctx->tpl_pool;

    // Apply workaround for tox positional arguments
    char *toxconf = NULL;
    if (!access("tox.ini", F_OK)) {
        if (!fix_tox_conf("tox.ini", &toxconf, PATH_MAX, tpl)) {
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
                guard_free(basetemp_path);
                return -3;
            }
            const char *tox_target = f->argv[0].t_char_ptr;
            const char *pytest_args = f->argv[1].t_char_ptr;
            if (isempty(toxconf) || !strcmp(toxconf, "/")) {
                SYSERROR("Unsafe toxconf path: '%s'", toxconf);
                guard_free(basetemp_path);
                guard_free(jxml_path);
                return -4;
            }
            snprintf(*output, STASIS_BUFSIZ - 1, "\npip install tox && (tox -e py%s%s -c %s --root . -- --basetemp=\"%s\" --junitxml=\"%s\" %s ; rm -f '%s')\n", ctx->meta.python_compact, tox_target, toxconf, basetemp_path, jxml_path, pytest_args ? pytest_args : "", toxconf);

            guard_free(jxml_path);
            guard_free(basetemp_path);
        }
    }
    return 0;
}