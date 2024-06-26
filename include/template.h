//! @file template.h
#ifndef STASIS_TEMPLATE_H
#define STASIS_TEMPLATE_H

#include "core.h"

/**
 * Map a text value to a pointer in memory
 *
 * @param key in-text variable name
 * @param ptr pointer to string
 */
void tpl_register(char *key, char **ptr);

/**
 * Free the template engine
 */
void tpl_free();

/**
 * Retrieve the value of a key mapped by the template engine
 * @param key string registered by `tpl_register`
 * @return a pointer to value, or NULL if the key is not present
 */
char *tpl_getval(char *key);

/**
 * Replaces occurrences of all registered key value pairs in `str`
 * @param str the text data to render
 * @return a rendered copy of `str`, or NULL.
 * The caller is responsible for free()ing memory allocated by this function
 */
char *tpl_render(char *str);

/**
 * Write tpl_render() output to a file
 * @param str the text to render
 * @param filename the output file name
 * @return 0 on success, <0 on error
 */
int tpl_render_to_file(char *str, const char *filename);

typedef int tplfunc(void *frame, void *result);

struct tplfunc_frame {
    char *key;
    tplfunc *func;
    int argc;
    union {
        char **t_char_refptr;
        char *t_char_ptr;
        void *t_void_ptr;
        int *t_int_ptr;
        unsigned *t_uint_ptr;
        float *t_float_ptr;
        double *t_double_ptr;
        char t_char;
        int t_int;
        unsigned t_uint;
        float t_float;
        double t_double;
    } argv[10]; // accept up to 10 arguments
};

/**
 * Register a template function
 * @param key function name to expose to "func:" interface
 * @param tplfunc_ptr pointer to function of type tplfunc
 * @param argc number of function arguments to accept
 */
void tpl_register_func(char *key, void *tplfunc_ptr, int argc);

/**
 * Get the function frame associated with a template function
 * @param key function name
 * @return tplfunc_frame structure
 */
struct tplfunc_frame *tpl_getfunc(char *key);

#endif //STASIS_TEMPLATE_H
