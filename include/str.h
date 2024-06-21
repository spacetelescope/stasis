/**
 * @file str.h
 */
#ifndef STASIS_STR_H
#define STASIS_STR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "core.h"

#define STASIS_SORT_ALPHA 1 << 0
#define STASIS_SORT_NUMERIC 1 << 1
#define STASIS_SORT_LEN_ASCENDING 1 << 2
#define STASIS_SORT_LEN_DESCENDING 1 << 3

/**
 * Determine how many times the character `ch` appears in `sptr` string
 * @param sptr string to scan
 * @param ch character to find
 * @return count of characters found
 */
int num_chars(const char *sptr, int ch);

/**
 * Scan for `pattern` string at the beginning of `sptr`
 *
 * @param sptr string to scan
 * @param pattern string to search for
 * @return 1 = found, 0 = not found / error
 */
int startswith(const char *sptr, const char *pattern);

/**
 * Scan for `pattern` string at the end of `sptr`
 *
 * @param sptr string to scan
 * @param pattern string to search for
 * @return 1 = found, 0 = not found / error
 */
int endswith(const char *sptr, const char *pattern);

/**
 * Deletes any characters matching `chars` from `sptr` string
 *
 * @param sptr string to be modified in-place
 * @param chars a string containing characters (e.g. " \n" would delete whitespace and line feeds)
 */
void strchrdel(char *sptr, const char *chars);

/**
 * Split a string by every delimiter in `delim` string.
 *
 * Callee should free memory using `GENERIC_ARRAY_FREE()`
 *
 * @param sptr string to split
 * @param delim characters to split on
 * @return success=parts of string, failure=NULL
 */
char** split(char *sptr, const char* delim, size_t max);

/**
 * Create new a string from an array of strings
 *
 * ~~~{.c}
 * char *array[] = {
 *     "this",
 *     "is",
 *     "a",
 *     "test",
 *     NULL,
 * }
 *
 * char *test = join(array, " ");    // "this is a test"
 * char *test2 = join(array, "_");   // "this_is_a_test"
 * char *test3 = join(array, ", ");  // "this, is, a, test"
 *
 * free(test);
 * free(test2);
 * free(test3);
 * ~~~
 *
 * @param arr
 * @param separator characters to insert between elements in string
 * @return new joined string
 */
char *join(char **arr, const char *separator);

/**
 * Join two or more strings by a `separator` string
 * @param separator
 * @param ...
 * @return string
 */
char *join_ex(char *separator, ...);

/**
 * Extract the string encapsulated by characters listed in `delims`
 *
 * ~~~{.c}
 * char *str = "this is [some data] in a string";
 * char *data = substring_between(string, "[]");
 * // data = "some data";
 * ~~~
 *
 * @param sptr string to parse
 * @param delims two characters surrounding a string
 * @return success=text between delimiters, failure=NULL
 */
char *substring_between(char *sptr, const char *delims);

/**
 * Sort an array of strings
 * @param arr a NULL terminated array of strings
 * @param sort_mode
 *     - STASIS_SORT_LEN_DESCENDING
 *     - STASIS_SORT_LEN_ASCENDING
 *     - STASIS_SORT_ALPHA
 *     - STASIS_SORT_NUMERIC
 */
void strsort(char **arr, unsigned int sort_mode);

/**
 * Determine whether the input character is a relational operator
 * Note: `~` is non-standard
 * @param ch
 * @return 0=no, 1=yes
 */
int isrelational(char ch);

/**
 * Print characters in `s`, `len` times
 * @param s
 * @param len
 */
void print_banner(const char *s, int len);

/**
 * Search for string in an array of strings
 * @param arr array of strings
 * @param str string to search for
 * @return yes=`pointer to string`, no=`NULL`, failure=`NULL`
 */
char *strstr_array(char **arr, const char *str);

/**
 * Remove duplicate strings from an array of strings
 * @param arr
 * @return success=array of unique strings, failure=NULL
 */
char **strdeldup(char **arr);

/** Remove leading whitespace from a string
 *
 * ~~~{.c}
 * char input[100];
 *
 * strcpy(input, "          I had leading spaces");
 * lstrip(input);
 * // input is now "I had leading spaces"
 * ~~~
 * @param sptr pointer to string
 * @return pointer to first non-whitespace character in string
 */
char *lstrip(char *sptr);

/**
 * Strips trailing whitespace from a given string
 *
 * ~~~{.c}
 * char input[100];
 *
 * strcpy(input, "I had trailing spaces          ");
 * strip(input);
 * // input is now "I had trailing spaces"
 * ~~~
 *
 * @param sptr input string
 * @return truncated string
 */
char *strip(char *sptr);

/**
 * Check if a given string is "visibly" empty
 *
 * ~~~{.c}
 * char visibly[100];
 *
 * strcpy(visibly, "\t     \t\n");
 * if (isempty(visibly)) {
 *     printf("string is 'empty'\n");
 * } else {
 *     printf("string is not 'empty'\n");
 * }
 * ~~~
 *
 * @param sptr pointer to string
 * @return 0=not empty, 1=empty
 */
int isempty(char *sptr);

/**
 * Determine if a string is encapsulated by quotes
 * @param sptr pointer to string
 * @return 0=not quoted, 1=quoted
 */
int isquoted(char *sptr);

/**
 * Collapse whitespace in `s`. The string is modified in place.
 * @param s
 * @return pointer to `s`
 */
char *normalize_space(char *s);

/**
 * Duplicate an array of strings
 *
 * ~~~{.c}
 * char **array_orig = calloc(10, sizeof(*orig));
 * orig[0] = strdup("one");
 * orig[1] = strdup("two");
 * orig[2] = strdup("three");
 * // ...
 * char **array_orig_copy = strdup_array(orig);
 *
 * for (size_t i = 0; array_orig_copy[i] != NULL; i++) {
 *     printf("array_orig[%zu] = '%s'\narray_orig_copy[%zu] = '%s'\n\n",
 *            i, array_orig[i],
 *            i, array_orig_copy[i]);
 *     free(array_orig_copy[i]);
 *     free(array_orig[i]);
 * }
 * free(array_orig_copy);
 * free(array_orig);
 *
 * ~~~
 *
 * @param array
 * @return
 */
char **strdup_array(char **array);

/**
 * Compare an array of strings
 *
 * ~~~{.c}
 * const char *a[] = {
 *     "I",
 *     "like",
 *     "computers."
 * };
 * const char *b[] = {
 *     "I",
 *     "like",
 *     "cars."
 * };
 * if (!strcmp_array(a, b)) {
 *     printf("a and b are not equal\n");
 * } else {
 *     printf("a and b are equal\n");
 * }
 * ~~~
 *
 * @param a pointer to array
 * @param b poitner to array
 * @return 0 on identical, non-zero for different
 */
int strcmp_array(const char **a, const char **b);

/**
 * Determine whether a string is comprised of digits
 * @param s
 * @return 0=no, 1=yes
 */
int isdigit_s(const char *s);

/**
 * Convert input string to lowercase
 *
 * ~~~{.c}
 * char *str = strdup("HELLO WORLD!");
 * tolower_s(str);
 * // str is "hello world!"
 * ~~~
 *
 * @param s input string
 * @return pointer to input string
 */
char *tolower_s(char *s);

/**
 * Return a copy of the input string with "." characters removed
 *
 * ~~~{.c}
 * char *version = strdup("1.2.3");
 * char *version_short = to_short_version(str);
 * // version_short is "123"
 * free(version_short);
 *
 * ~~~
 *
 * @param s input string
 * @return pointer to new string
 */
char *to_short_version(const char *s);

#endif //STASIS_STR_H
