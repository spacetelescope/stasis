//
// Created by jhunk on 12/17/23.
//

#include "template.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "utils.h"

struct tplfunc_frame *tpl_pool_func[1024] = {0};
unsigned tpl_pool_func_used = 0;

extern void tpl_reset(struct tpl_pool **list) {
    SYSDEBUG("Resetting template engine");
    tpl_free(list);
}

struct tpl_pool *tpl_init() {
    struct tpl_pool *list = calloc(1, sizeof(*list));
    if (!list) {
        SYSERROR("Unable to allocate memory for tpl pool");
        return NULL;
    }
    list->data = calloc(1, sizeof(**list->data));
    if (!list->data) {
        SYSERROR("unable to allocate memory for tpl data");
        return NULL;
    }
    return list;
}

struct tpl_pool *tpl_copy(struct tpl_pool *src) {
    struct tpl_pool *list = tpl_init();
    if (!list) {
        SYSERROR("Unable to allocate memory for tpl pool");
        return NULL;
    }
    for (size_t i = 0; i < src->used; i++) {
        const struct tpl_item *item = src->data[i];
        tpl_register(&list, item->key, item->ptr);
    }

    return list;
}

void tpl_register_func(char *key, tplfunc *tplfunc_ptr, int argc, void *data_in) {
    struct tplfunc_frame *frame = calloc(1, sizeof(*frame));
    if (!frame) {
        SYSERROR("unable to allocate memory for function frame");
        exit(1);
    }

    frame->key = strdup(key);
    if (!frame->key) {
        SYSERROR("unable to allocate memory for function frame key");
        exit(1);
    }
    frame->argc = argc;
    frame->func = tplfunc_ptr;
    frame->data_in = data_in;
    SYSDEBUG("Registering function:\n\tkey=%s\n\targc=%d\n\tfunc=%p\n\tdata_in=%p", frame->key, frame->argc, frame->func, frame->data_in);

    tpl_pool_func[tpl_pool_func_used] = frame;
    tpl_pool_func_used++;
}

int tpl_key_exists(struct tpl_pool **list, char *key) {
    for (size_t i = 0; i < (*list)->used; i++) {
        if ((*list)->data[i]->key) {
            if (!strcmp((*list)->data[i]->key, key)) {
                return true;
            }
        }
    }
    return false;
}

void tpl_register(struct tpl_pool **list, char *key, char **ptr) {
    struct tpl_item *item = NULL;
    int replacing = 0;

    if (tpl_key_exists(list, key)) {
        for (size_t i = 0; i < (*list)->used; i++) {
            if ((*list)->data[i] && (*list)->data[i]->key) {
                if (!strcmp((*list)->data[i]->key, key)) {
                    item = (*list)->data[i];
                    break;
                }
            }
        }
        replacing = 1;
    } else {
        item = calloc(1, sizeof(*item));
        if (!item) {
            SYSERROR("unable to allocate memory for new item");
            exit(1);
        }
        item->key = strdup(key);
        if (!key) {
            SYSERROR("unable to allocate memory for new key");
            exit(1);
        }
    }

    if (!item) {
        SYSERROR("unable to register tpl_item for %s", key);
        exit(1);
    }

    item->ptr = ptr;
    if (!replacing) {
        SYSDEBUG("Registered tpl_item at index %u:\n\tkey=%s\n\tptr=%s", (*list)->used, item->key, *item->ptr ? *item->ptr : "NULL");
        (*list)->allocated++;
        const size_t newsize = sizeof((*list)->data) * (*list)->allocated;
        struct tpl_item **tmp = realloc((*list)->data, newsize);
        if (!tmp) {
            SYSERROR("unable to extend tpl_pool record count to %zu", (*list)->allocated);
            exit(1);
        }
        (*list)->data = tmp;
        (*list)->data[(*list)->used] = item;
        (*list)->used++;
    }
}

void tpl_free_func_pool() {
    for (unsigned i = 0; i < tpl_pool_func_used; i++) {
        struct tplfunc_frame *item = tpl_pool_func[i];
        guard_free(item->key);
        guard_free(item);
    }
}

void tpl_free(struct tpl_pool **list) {
    struct tpl_pool *x = *list;
    if (!x) {
        return;
    }
    if (!x->data) {
        return;
    }
    for (size_t i = 0; i < (*list)->used; i++) {
        guard_free((*list)->data[i]->key);
    }
    guard_array_n_free(x->data, (*list)->used);
    guard_free(x);
}

char *tpl_getval(struct tpl_pool **list, char *key) {
    char *result = NULL;
    for (size_t i = 0; i < (*list)->used; i++) {
        if ((*list)->data[i]->key) {
            if (!strcmp((*list)->data[i]->key, key)) {
                result = *(*list)->data[i]->ptr;
                break;
            }
        }
    }
    return result;
}

struct tplfunc_frame *tpl_getfunc(char *key) {
    struct tplfunc_frame *result = NULL;
    for (size_t i = 0; i < tpl_pool_func_used; i++) {
        if (tpl_pool_func[i]->key) {
            if (!strcmp(tpl_pool_func[i]->key, key)) {
                result = tpl_pool_func[i];
                break;
            }
        }
    }
    return result;
}

char *tpl_render(struct tpl_pool **list, char *str) {
    if (!str) {
        return NULL;
    } else if (!strlen(str)) {
        return strdup("");
    }
    size_t output_bytes = 1024 + strlen(str); // TODO: Is grow working correctly?
    char *output = NULL;
    char *b_close = NULL;
    char *pos = NULL;
    pos = str;

    output = calloc(output_bytes, sizeof(*output));
    if (!output) {
        SYSERROR("unable to allocate output buffer: %s", strerror(errno));
        return NULL;
    }

    for (size_t off = 0, z = 0; off < strlen(str); off++) {
        char key[255] = {0};
        char *value = NULL;

        memset(key, 0, sizeof(key));
        grow(z, &output_bytes, &output);
        // At opening brace
        if (!strncmp(&pos[off], "{{", 2)) {
            // Scan until key is reached
            while (!isalnum(pos[off])) {
                off++;
            }

            // Read key name
            size_t key_len = 0;
            while (isalnum(pos[off]) || pos[off] != '}') {
                if (isspace(pos[off]) || isblank(pos[off])) {
                    // skip whitespace in key
                    off++;
                    continue;
                }
                key[key_len] = pos[off];
                key_len++;
                off++;
            }

            char *type_stop = NULL;
            type_stop = strchr(key, ':');

            int do_env = 0;
            int do_func = 0;
            if (type_stop) {
                if (!strncmp(key, "env", type_stop - key)) {
                    do_env = 1;
                } else if (!strncmp(key, "func", type_stop - key)) {
                    do_func = 1;
                }
            }

            // Find closing brace
            b_close = strstr(&pos[off], "}}");
            if (!b_close) {
                SYSERROR("while templating '%s'\n\nunbalanced brace at position %zu", str, z);
                guard_free(output);
                return NULL;
            } else {
                // Jump past closing brace
                off = ((b_close + 2) - pos);
            }

            if (do_env) { // {{ env:VAR }}
                char *k = type_stop + 1;
                size_t klen = strlen(k);
                memmove(key, k, klen);
                key[klen] = 0;
                char *env_val = getenv(key);
                value = strdup(env_val ? env_val : "");
            } else if (do_func) { // {{ func:NAME(a, ...) }}
                char func_name_temp[STASIS_NAME_MAX] = {0};
                safe_strncpy(func_name_temp, type_stop + 1, sizeof(func_name_temp));

                char *param_begin = strchr(func_name_temp, '(');
                if (!param_begin) {
                    SYSERROR("At position %zu in %s\nfunction name must be followed by a '('", off, key);
                    guard_free(output);
                    return NULL;
                }
                *param_begin = 0;
                param_begin++;
                char *param_end = strrchr(param_begin, ')');
                if (!param_end) {
                    SYSERROR("At position %zu in %s\nfunction arguments must be closed with a ')'", off, key);
                    guard_free(output);
                    return NULL;
                }
                *param_end = 0;
                char *k = func_name_temp;
                char **params = split(param_begin, ",", 0);
                int params_count;
                for (params_count = 0; params[params_count] != NULL; params_count++) {}

                struct tplfunc_frame *frame = tpl_getfunc(k);
                if (!frame) {
                    SYSERROR("no function named '%s'", k);
                    guard_array_n_free(params, (size_t) params_count);
                    return NULL;
                }
                if (params_count > frame->argc || params_count < frame->argc) {
                    SYSERROR("At position %zu in %s\nIncorrect number of arguments for function: %s (expected %d, got %d)", off, key, frame->key, frame->argc, params_count);
                    value = strdup("");
                } else {
                    for (size_t p = 0; p < sizeof(frame->argv) / sizeof(*frame->argv) && params[p] != NULL; p++) {
                        lstrip(params[p]);
                        strip(params[p]);
                        frame->argv[p].t_char_ptr = params[p];
                    }
                    char *func_result = NULL;
                    int func_status = 0;
                    if ((func_status = frame->func(frame, &func_result))) {
                        SYSERROR("%s returned non-zero status: %d", frame->key, func_status);
                    }
                    value = strdup(func_result ? func_result : "");
                    guard_free(func_result);
                }
                guard_array_free(params);
            } else {
                // Read replacement value
                value = strdup(tpl_getval(list, key) ? tpl_getval(list, key) : "");
            }
        }

        if (value) {
            // Set output iterator to end of replacement value
            z += strlen(value);

            // Append replacement value
            grow(z, &output_bytes, &output);
            safe_strncat(output, value, output_bytes);
            guard_free(value);
            output[z] = 0;
        }

        output[z] = pos[off];
        z++;
    }
    //SYSDEBUG("template output length: %zu", strlen(output));
    //SYSDEBUG("template output bytes: %zu", output_bytes);
    return output;
}

int tpl_render_to_file(struct tpl_pool **list, char *str, const char *filename) {
    // Render the input string
    char *result = tpl_render(list, str);
    if (!result) {
        return -1;
    }

    // Open the destination file for writing
    SYSDEBUG("Rendering to %s", filename);
    FILE *fp = fopen(filename, "w+");
    if (!fp) {
        guard_free(result);
        return -1;
    }

    // Write rendered string to file
    fprintf(fp, "%s", result);
    fclose(fp);
    SYSDEBUG("Rendered successfully");

    guard_free(result);
    return 0;
}