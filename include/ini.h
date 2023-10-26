#ifndef OHMYCAL_INI_H
#define OHMYCAL_INI_H
#include <stddef.h>
#include <stdbool.h>

#define INIVAL_TYPE_INT 1
#define INIVAL_TYPE_UINT 2
#define INIVAL_TYPE_LONG 3
#define INIVAL_TYPE_ULONG 4
#define INIVAL_TYPE_LLONG 5
#define INIVAL_TYPE_ULLONG 6
#define INIVAL_TYPE_DOUBLE 7
#define INIVAL_TYPE_FLOAT 8
#define INIVAL_TYPE_STR 9
#define INIVAL_TYPE_STR_ARRAY 10
#define INIVAL_TYPE_BOOL 11

#define INIVAL_TO_LIST 1 << 1

union INIVal {
    int as_int;
    unsigned as_uint;
    long as_long;
    unsigned long as_ulong;
    long long as_llong;
    unsigned long long as_ullong;
    double as_double;
    float as_float;
    char *as_char_p;
    char **as_char_array_p;
    bool as_bool;
};


struct INIData {
    char *key;
    char *value;
};
struct INISection {
    size_t data_count;
    char *key;
    struct INIData **data;
};
struct INIFILE {
    size_t section_count;
    struct INISection **section;
};

struct INIFILE *ini_open(const char *filename);
struct INIData *ini_getall(struct INIFILE *ini, char *section_name);
int ini_getval(struct INIFILE *ini, char *section_name, char *key, int type, union INIVal *result);
void ini_show(struct INIFILE *ini);
void ini_free(struct INIFILE **ini);
#endif //OHMYCAL_INI_H
