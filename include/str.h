/**
 * @file str.h
 */
#ifndef OMC_STR_H
#define OMC_STR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "omc.h"

#define OMC_SORT_ALPHA 1 << 0
#define OMC_SORT_NUMERIC 1 << 1
#define OMC_SORT_LEN_ASCENDING 1 << 2
#define OMC_SORT_LEN_DESCENDING 1 << 3

int num_chars(const char *sptr, int ch);
int startswith(const char *sptr, const char *pattern);
int endswith(const char *sptr, const char *pattern);
char *normpath(const char *path);
void strchrdel(char *sptr, const char *chars);
long int strchroff(const char *sptr, int ch);
void strdelsuffix(char *sptr, const char *suffix);
char** split(char *sptr, const char* delim, size_t max);
char *join(char **arr, const char *separator);
char *join_ex(char *separator, ...);
char *substring_between(char *sptr, const char *delims);
void strsort(char **arr, unsigned int sort_mode);
int isrelational(char ch);
void print_banner(const char *s, int len);
char *strstr_array(char **arr, const char *str);
char **strdeldup(char **arr);
char *lstrip(char *sptr);
char *strip(char *sptr);
int isempty(char *sptr);
int isquoted(char *sptr);
char *normalize_space(char *s);
char **strdup_array(char **array);
int strcmp_array(const char **a, const char **b);
int isdigit_s(const char *s);
char *tolower_s(char *s);
char *to_short_version(const char *s);

#endif //OMC_STR_H
