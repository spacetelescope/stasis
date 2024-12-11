/**
 * String array convenience functions
 * @file strlist.h
 */
#ifndef STASIS_STRLIST_H
#define STASIS_STRLIST_H

typedef int (ReaderFn)(size_t line, char **);

#include <stdlib.h>
#include "core.h"
#include "utils.h"
#include "str.h"


struct StrList {
    size_t num_alloc;
    size_t num_inuse;
    char **data;
};

struct StrList *strlist_init();
void strlist_remove(struct StrList *pStrList, size_t index);
long double strlist_item_as_long_double(struct StrList *pStrList, size_t index);
double strlist_item_as_double(struct StrList *pStrList, size_t index);
float strlist_item_as_float(struct StrList *pStrList, size_t index);
unsigned long long strlist_item_as_ulong_long(struct StrList *pStrList, size_t index);
long long strlist_item_as_long_long(struct StrList *pStrList, size_t index);
unsigned long strlist_item_as_ulong(struct StrList *pStrList, size_t index);
long strlist_item_as_long(struct StrList *pStrList, size_t index);
unsigned int strlist_item_as_uint(struct StrList *pStrList, size_t index);
int strlist_item_as_int(struct StrList *pStrList, size_t index);
unsigned short strlist_item_as_ushort(struct StrList *pStrList, size_t index);
short strlist_item_as_short(struct StrList *pStrList, size_t index);
unsigned char strlist_item_as_uchar(struct StrList *pStrList, size_t index);
char strlist_item_as_char(struct StrList *pStrList, size_t index);
char *strlist_item_as_str(struct StrList *pStrList, size_t index);
char *strlist_item(struct StrList *pStrList, size_t index);
void strlist_set(struct StrList **pStrList, size_t index, char *value);
size_t strlist_count(struct StrList *pStrList);
void strlist_reverse(struct StrList *pStrList);
void strlist_sort(struct StrList *pStrList, unsigned int mode);
int strlist_append_file(struct StrList *pStrList, char *path, ReaderFn *readerFn);
void strlist_append_strlist(struct StrList *pStrList1, struct StrList *pStrList2);
void strlist_append(struct StrList **pStrList, char *str);
void strlist_append_array(struct StrList *pStrList, char **arr);
void strlist_append_tokenize(struct StrList *pStrList, char *str, char *delim);
struct StrList *strlist_copy(struct StrList *pStrList);
int strlist_cmp(struct StrList *a, struct StrList *b);
void strlist_free(struct StrList **pStrList);

#define STRLIST_E_SUCCESS 0
#define STRLIST_E_OUT_OF_RANGE 1
#define STRLIST_E_INVALID_VALUE 2
#define STRLIST_E_UNKNOWN 3
extern int strlist_errno;
const char *strlist_get_error(int flag);


#endif //STASIS_STRLIST_H
