#include "core.h"
#include "website.h"

int indexer_make_website(const struct Delivery *ctx) {
    char *css_filename = calloc(PATH_MAX, sizeof(*css_filename));
    if (!css_filename) {
        SYSERROR("unable to allocate string for CSS file path: %s", strerror(errno));
        return -1;
    }

    sprintf(css_filename, "%s/%s", globals.sysconfdir, "stasis_pandoc.css");
    const int have_css = access(css_filename, F_OK | R_OK) == 0;

    struct StrList *dirs = strlist_init();
    strlist_append(&dirs, ctx->storage.delivery_dir);
    strlist_append(&dirs, ctx->storage.results_dir);

    struct StrList *inputs = NULL;
    for (size_t i = 0; i < strlist_count(dirs); i++) {
        const char *pattern = "*.md";
        char *dirpath = strlist_item(dirs, i);
        if (get_files(&inputs, dirpath, pattern)) {
            SYSERROR("%s does not contain files with pattern: %s", dirpath, pattern);
            continue;
        }

        char *root = strlist_item(dirs, i);
        for (size_t x = 0; x < strlist_count(inputs); x++) {
            char *filename = path_basename(strlist_item(inputs, x));
            char fullpath_src[PATH_MAX] = {0};
            char fullpath_dest[PATH_MAX] = {0};
            sprintf(fullpath_src, "%s/%s", root, filename);
            if (access(fullpath_src, F_OK)) {
                continue;
            }

            // Replace *.md extension with *.html.
            strcpy(fullpath_dest, fullpath_src);
            gen_file_extension_str(fullpath_dest, ".html");

            // Convert markdown to html
            if (pandoc_exec(fullpath_src, fullpath_dest, have_css ? css_filename : NULL, "STASIS")) {
                msg(STASIS_MSG_L2 | STASIS_MSG_WARN, "Unable to convert %s\n", fullpath_src);
            }

            if (file_replace_text(fullpath_dest, ".md", ".html", 0)) {
                // inform-only
                SYSERROR("%s: failed to rewrite *.md urls with *.html extension", fullpath_dest);
            }

            // Link the nearest README.html to index.html
            if (!strcmp(filename, "README.md")) {
                char link_from[PATH_MAX] = {0};
                char link_dest[PATH_MAX] = {0};
                strcpy(link_from, "README.html");
                sprintf(link_dest, "%s/%s", root, "index.html");
                if (symlink(link_from, link_dest)) {
                    SYSERROR("Warning: symlink(%s, %s) failed: %s", link_from, link_dest, strerror(errno));
                }
            }
        }
        guard_strlist_free(&inputs);
    }
    guard_free(css_filename);
    guard_strlist_free(&dirs);

    return 0;
}
