#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"
#include "utils.h"
#include "system.h"
#include "github.h"

struct GHContent {
    char *data;
    size_t len;
};

static size_t writer(const void *contents, size_t size, size_t nmemb, void *result) {
    const size_t newlen = size * nmemb;
    struct GHContent *content = (struct GHContent *) result;

    char *ptr = realloc(content->data, content->len + newlen + 1);
    if (!ptr) {
        SYSERROR("realloc failed");
        return 0;
    }

    content->data = ptr;
    memcpy(&(content->data[content->len]), contents, newlen);
    content->len += newlen;
    content->data[content->len] = 0;

    return newlen;
}

static char *unescape_lf(char *value) {
    char *seq = strstr(value, "\\n");
    while (seq != NULL) {
        size_t cur_len = strlen(seq);
        memmove(seq, seq + 1, strlen(seq) - 1);
        *seq = '\n';
        if (strlen(seq) && cur_len) {
            seq[cur_len - 1] = 0;
        }
        seq = strstr(value, "\\n");
    }
    return value;
}

int get_github_release_notes(const char *api_token, const char *repo, const char *tag, const char *target_commitish, char **output) {
    const char *endpoint_header_auth_fmt = "Authorization: Bearer %s";
    const char *endpoint_header_api_version = "X-GitHub-Api-Version: " STASIS_GITHUB_API_VERSION;
    const char *endpoint_post_fields_fmt = "{\"tag_name\":\"%s\", \"target_commitish\":\"%s\"}";
    const char *endpoint_url_fmt = "https://api.github.com/repos/%s/releases/generate-notes";
    char endpoint_header_auth[PATH_MAX] = {0};
    char endpoint_post_fields[PATH_MAX] = {0};
    char endpoint_url[PATH_MAX] = {0};
    struct curl_slist *list = NULL;
    struct GHContent content;

    CURL *curl = curl_easy_init();
    if (!curl) {
        return -1;
    }

    // Render the header data
    snprintf(endpoint_header_auth, sizeof(endpoint_header_auth), endpoint_header_auth_fmt, api_token);
    snprintf(endpoint_post_fields, sizeof(endpoint_post_fields), endpoint_post_fields_fmt, tag, target_commitish);
    snprintf(endpoint_url, sizeof(endpoint_url), endpoint_url_fmt, repo);

    // Begin curl configuration
    curl_easy_setopt(curl, CURLOPT_URL, endpoint_url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, endpoint_post_fields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &content);

    // Append headers to the request
    list = curl_slist_append(list, "Accept: application/vnd.github+json");
    list = curl_slist_append(list, endpoint_header_auth);
    list = curl_slist_append(list, endpoint_header_api_version);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

    // Set the user-agent (github requires one)
    char user_agent[20] = {0};
    snprintf(user_agent, sizeof(user_agent), "stasis/%s", VERSION);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);

    // Execute curl request
    memset(&content, 0, sizeof(content));
    CURLcode res = curl_easy_perform(curl);

    // Clean up
    curl_slist_free_all(list);
    curl_easy_cleanup(curl);

    if(res != CURLE_OK) {
        SYSERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));
        return -1;
    }

    // Replace all "\\n" literals with new line characters
    char *line = unescape_lf(content.data);
    if (line) {
        const char *field_message = "\"message\":\"";
        const char *field_body = "\"body\":\"";
        char *data_offset = NULL;
        if ((data_offset = strstr(line, field_body))) {
            // Skip past the body field
            data_offset += strlen(field_body);
            // Remove quotation mark (and trailing comma if it exists)
            int trim = 2;
            char last_char = data_offset[strlen(data_offset) - trim];
            if (last_char == ',') {
                trim++;
            }
            // Truncate the trimmed bytes
            memset(&data_offset[strlen(data_offset) - trim], 0, trim);
            // Extract release notes
            *output = calloc(strlen(data_offset) + 1, sizeof(**output));
            // Copy output (including terminator)
            safe_strncpy(*output, data_offset, strlen(data_offset) + 1);
        } else if ((data_offset = strstr(line, field_message))) {
            // Skip past the message field
            data_offset += strlen(field_message);
            char *data_mark = strchr(data_offset, '"');
            if (data_mark) {
                *data_mark = '\0';
            }
            SYSERROR("GitHub API Error: '%s'", data_offset);
            SYSERROR("URL: %s", endpoint_url);
            SYSERROR("POST: %s", endpoint_post_fields);
            guard_free(content.data);
            return -1;
        }
    } else {
        SYSERROR("Unknown error");
        guard_free(content.data);
        return -1;
    }

    guard_free(content.data);
    return 0;
}

char *get_git_describe(const char *repopath, const char *ref) {
    if (!pushd(repopath)) {
        char cmd[8192] = {0};
        int cmd_status = 0;
        snprintf(cmd, sizeof(cmd), "git describe --tags --always --long %s", ref);
        char *result = shell_output(cmd, &cmd_status);
        if (!result || cmd_status) {
            popd();
            return NULL;
        }
        strip(result);
        popd();
        return result;
    }
    SYSERROR("unable to change directory: %s", repopath);
    return NULL;
}

char *get_git_tag(const char *repopath, const char *ref, const unsigned parent) {
    if (!pushd(repopath)) {
        char cmd[8192] = {0};
        int cmd_status = 0;

        // If the repository has no tags, return NULL;
        char *tags = shell_output("git tag -l", &cmd_status);
        if (tags && cmd_status == 0) {
            if (num_chars(tags, '\n') == 0) {
                guard_free(tags);
                popd();
                return NULL;
            }
            guard_free(tags);
        }

        const char *fmt = "git describe --tags --abbrev=0 %s";
        snprintf(cmd, sizeof(cmd), fmt, ref);

        // Resolve tag from ref. --abbrev=0 omits the suffix '-NUM-gHASH'
        char *resolved = shell_output(cmd, &cmd_status);
        if (!resolved || cmd_status) {
            guard_free(resolved);
            popd();
            return NULL;
        }

        char *output = NULL;
        if (resolved) {
            strip(resolved);
            fmt = "git describe --tags --abbrev=0 %s^%u";
            snprintf(cmd, sizeof(cmd), fmt, resolved, parent);
            guard_free(resolved);

            SYSDEBUG("Executing: %s", cmd);
            output = shell_output(cmd, &cmd_status);
            SYSDEBUG("Returned: %d", cmd_status);
            if (!output || cmd_status) {
                popd();
                return NULL;
            }
            strip(output);
        }
        popd();

        return output;
    }
    SYSERROR("unable to change directory: %s", repopath);
    return NULL;
}

char *get_git_commit_root(const char *repo_path, const char *ref) {
    if (!pushd(repo_path)) {
        char cmd[8192] = {0};
        int cmd_status = 0;
        snprintf(cmd, sizeof(cmd), "git rev-list --max-parents=0 %s", ref);
        char *output = shell_output(cmd, &cmd_status);
        if (!output || cmd_status) {
            guard_free(output);
            popd();
            return NULL;
        }
        strip(output);
        popd();
        return output;
    }
    SYSERROR("directory change failed: %s", repo_path);
    return NULL;
}

#define CMD_FMT_GIT_LOG "git log --pretty=format:'- %%s by %%aN'"
#define CMD_FMT_GIT_LOG_ADDENDUM_PR "--notes -E --grep='\\(#[0-9]+\\)'"
int get_git_release_notes(const char *repo_path, const char *tag, const char *target_commitish, char **output) {
    if (!target_commitish) {
        SYSERROR("target_commitish cannot be NULL");
        return -1;
    }
    if (!pushd(repo_path)) {
        char cmd[STASIS_BUFSIZ] = {0};
        int cmd_status = 0;
        char *target_commitish_as_tag = NULL;
        if (strcmp(target_commitish, "HEAD") != 0) {
            snprintf(cmd, sizeof(cmd), "git describe --tags --abbrev=0 %s", target_commitish);
            SYSDEBUG("Executing: %s", cmd);
            target_commitish_as_tag = shell_output(cmd, &cmd_status);
            if (!target_commitish_as_tag) {
                SYSERROR("unable to allocate memory for target_commitish_as_tag");
                popd();
                return -1;
            }
            strip(target_commitish_as_tag);
        }

        int use_head = 0;
        if (!target_commitish_as_tag) {
            use_head = 1;
            target_commitish_as_tag = strdup("HEAD");
        }

        // The default mode is to key on PR "(#n)" strings in the commit messages. This significantly reduces
        // clutter added by merge commits, etc.
        snprintf(cmd, sizeof(cmd), CMD_FMT_GIT_LOG " " CMD_FMT_GIT_LOG_ADDENDUM_PR);
        if (tag) {
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), " %s..%s", tag, target_commitish_as_tag);
        } else {
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), " %s", target_commitish_as_tag);
        }
        SYSDEBUG("Executing: %s", cmd);
        char *log_output = shell_output(cmd, &cmd_status);
        if (cmd_status) {
            if (log_output) {
                guard_free(log_output);
            }
            guard_free(target_commitish_as_tag);
            popd();
            return -1;
        }

        // When the repository has no PRs (single-developer projects) print everything in range instead.
        if (!num_chars(log_output, '\n')) {
            guard_free(log_output);
            if (tag) {
                snprintf(cmd, sizeof(cmd) - strlen(cmd), CMD_FMT_GIT_LOG " %s..%s", tag, target_commitish_as_tag);
            } else {
                snprintf(cmd, sizeof(cmd) - strlen(cmd), CMD_FMT_GIT_LOG " %s", target_commitish_as_tag);
            }
            SYSDEBUG("No commits? Trying again.");
            SYSDEBUG("Executing: %s", cmd);
            log_output = shell_output(cmd, &cmd_status);
            if (cmd_status) {
                if (log_output) {
                    guard_free(log_output);
                }
                guard_free(target_commitish_as_tag);
                popd();
                return -1;
            }

        }

        char *release_notes = NULL;
        char *head = target_commitish_as_tag;
        if (use_head) {
            head = get_git_describe(repo_path, target_commitish_as_tag);
        }

        const char *release_notes_fmt = NULL;
        int release_notes_failed = 0;
        if (tag) {
            release_notes_fmt = "%s\n\n**Range:** %s..%s";
            release_notes_failed = asprintf(&release_notes, release_notes_fmt, log_output, tag, head) < 0;
        } else {
            // Because we're printing the ENTIRE history up to target_commitish_as_tag...
            release_notes_fmt = "%s\n\n**Up to:** %s";
            release_notes_failed = asprintf(&release_notes, release_notes_fmt, log_output, head) < 0;
        }

        if (use_head) {
            guard_free(head);
        }

        if (release_notes_failed || !release_notes) {
            SYSERROR("unable to allocate memory for release notes output string\n");
            guard_free(target_commitish_as_tag);
            guard_free(log_output);
            popd();
            return -1;
        }
        guard_free(target_commitish_as_tag);
        guard_free(log_output);
        *output = release_notes;
        popd();
    } else {
        SYSERROR("directory change failed: %s", repo_path);
        return -1;
    }
    return 0;
}