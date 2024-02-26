//
// Created by jhunk on 12/17/23.
//

#include "template.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>


struct tpl_item {
    char *key;
    char **ptr;
};
struct tpl_item *tpl_pool[1024] = {0};
unsigned tpl_pool_used = 0;

void tpl_register(char *key, char **ptr) {
    struct tpl_item *item = NULL;
    item = calloc(1, sizeof(*item));
    if (!item) {
        perror("unable to register tpl_item");
        exit(1);
    }
    item->key = strdup(key);
    item->ptr = ptr;
    tpl_pool[tpl_pool_used] = item;
    tpl_pool_used++;
}

void tpl_free() {
    for (unsigned i = 0; i < tpl_pool_used; i++) {
        struct tpl_item *item = tpl_pool[i];
        if (item) {
            if (item->key) {
                free(item->key);
            }
            free(item);
        }
    }
}

char *tpl_getval(char *key) {
    char *result = NULL;
    for (size_t i = 0; i < tpl_pool_used; i++) {
        if (tpl_pool[i]->key) {
            if (!strcmp(tpl_pool[i]->key, key)) {
                result = *tpl_pool[i]->ptr;
                break;
            }
        }
    }
    return result;
}

static int grow(size_t z, size_t *output_bytes, char **output) {
    if (z >= *output_bytes) {
        size_t new_size = *output_bytes + z + 1;
#ifdef DEBUG
        fprintf(stderr, "template output buffer new size: %zu\n", *output_bytes);
#endif
        char *tmp = realloc(*output, new_size);
        if (!tmp) {
            perror("realloc failed");
            return -1;
        }
        memset(tmp + strlen(tmp), 0, new_size - strlen(tmp));
        *output = tmp;
        *output_bytes = new_size;
    }
    return 0;
}

char *tpl_render(char *str) {
    if (!str) {
        return NULL;
    }
    size_t output_bytes = strlen(str) * 2;
    char *output = NULL;
    char *b_close = NULL;
    char *pos = NULL;
    pos = str;

    output = calloc(output_bytes, sizeof(*output));
    if (!output) {
        perror("unable to allocate output buffer");
        return NULL;
    }

    size_t z = 0;
    while (*pos != 0) {
        char key[255] = {0};
        char *value = NULL;

        memset(key, 0, sizeof(key));
        grow(z, &output_bytes, &output);
        // At opening brace
        if (!strncmp(pos, "{{", 2)) {
            // Scan until key is reached
            while (!isalnum(*pos)) {
                pos++;
            }

            // Read key name
            size_t key_len = 0;
            while (isalnum(*pos) || *pos != '}') {
                if (isspace(*pos) || isblank(*pos)) {
                    break;
                }
                key[key_len] = *pos;
                key_len++;
                pos++;
            }

            char *type_stop = NULL;
            type_stop = strchr(key, ':');

            int do_env = 0;
            if (type_stop) {
                if (!strncmp(key, "env", type_stop - key)) {
                    do_env = 1;
                }
            }

            // Find closing brace
            b_close = strstr(pos, "}}");
            if (!b_close) {
                grow(z + strlen(pos), &output_bytes, &output);
                strcpy(output, pos);
                z += strlen(pos);
                continue;
            }
            // Jump past closing brace
            pos = b_close + 2;

            if (do_env) {
                char *k = type_stop + 1;
                size_t klen = strlen(k);
                memmove(key, k, klen);
                key[klen] = 0;
                char *env_val = getenv(key);
                value = strdup(env_val ? env_val : "");
            } else {
                // Read replacement value
                value = tpl_getval(key);
            }
        }

        if (value) {
            // Set output iterator to end of replacement value
            z += strlen(value);

            // Append replacement value
            grow(z, &output_bytes, &output);
            strcat(output, value);
        }

#ifdef DEBUG
        fprintf(stderr, "z=%zu, output_bytes=%zu\n", z, output_bytes);
#endif
        output[z] = *pos;
        pos++;
        z++;
    }
#ifdef DEBUG
    fprintf(stderr, "template output length: %zu\n", strlen(output));
    fprintf(stderr, "template output bytes: %zu\n", output_bytes);
#endif
    return output;
}

int tpl_render_to_file(char *str, const char *filename) {
    char *result;
    FILE *fp;

    // Render the input string
    result = tpl_render(str);
    if (!result) {
        return -1;
    }

    // Open the destination file for writing
    fp = fopen(filename, "w+");
    if (!fp) {
        guard_free(result)
        return -1;
    }

    // Write rendered string to file
    fprintf(fp, "%s", result);
    fclose(fp);

    guard_free(result);
    return 0;
}