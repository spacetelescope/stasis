#include "core.h"
#include "website.h"

int indexer_make_website(const struct Delivery *ctx) {
    if (!find_program("pandoc")) {
        fprintf(stderr, "pandoc is not installed: unable to generate HTML indexes\n");
        return 0;
    }

    char *css_filename = calloc(PATH_MAX, sizeof(*css_filename));
    if (!css_filename) {
        SYSERROR("unable to allocate string for CSS file path: %s", strerror(errno));
        return -1;
    }

    sprintf(css_filename, "%s/%s", globals.sysconfdir, "stasis_pandoc.css");
    const int have_css = access(css_filename, F_OK | R_OK) == 0;

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

    struct StrList *dirs = strlist_init();
    strlist_append(&dirs, ctx->storage.delivery_dir);
    strlist_append(&dirs, ctx->storage.results_dir);

    struct StrList *inputs = NULL;
    for (size_t i = 0; i < strlist_count(dirs); i++) {
        const char *pattern = "*.md";
        if (get_files(&inputs, ctx->storage.delivery_dir, pattern)) {
            SYSERROR("%s does not contain files with pattern: %s", ctx->storage.delivery_dir, pattern);
            guard_strlist_free(&inputs);
            continue;
        }
        char *root = strlist_item(dirs, i);
        for (size_t x = 0; x < strlist_count(inputs); x++) {
            char cmd[PATH_MAX] = {0};
            char *filename = strlist_item(inputs, x);
            char fullpath_src[PATH_MAX] = {0};
            char fullpath_dest[PATH_MAX] = {0};
            sprintf(fullpath_src, "%s/%s", root, filename);
            if (access(fullpath_src, F_OK)) {
                continue;
            }

            // Replace *.md extension with *.html.
            strcpy(fullpath_dest, fullpath_src);
            gen_file_extension_str(fullpath_dest, ".html");

            // Converts a markdown file to html
            strcpy(cmd, "pandoc ");
            strcat(cmd, pandoc_versioned_args);
            if (have_css) {
                strcat(cmd, "--css ");
                strcat(cmd, css_filename);
            }
            strcat(cmd, " ");
            strcat(cmd, "--metadata title=\"STASIS\" ");
            strcat(cmd, "-o ");
            strcat(cmd, fullpath_dest);
            strcat(cmd, " ");
            strcat(cmd, fullpath_src);
            if (globals.verbose) {
                puts(cmd);
            }
            // This might be negative when killed by a signal.
            // Otherwise, the return code is not critical to us.
            if (system(cmd) < 0) {
                guard_free(css_filename);
                guard_strlist_free(&dirs);
                return 1;
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
