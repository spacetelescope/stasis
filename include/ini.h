/// @file ini.h

#ifndef OMC_INI_H
#define OMC_INI_H
#include <stddef.h>
#include <stdbool.h>

#define INI_WRITE_RAW 0             ///< Dump INI data. Contents are not modified.
#define INI_WRITE_PRESERVE 1        ///< Dump INI data. Template strings are
#define INI_SETVAL_APPEND 0
#define INI_SETVAL_REPLACE 1
                                    ///< expanded to preserve runtime state.

#define INIVAL_TYPE_INT 1           ///< Integer
#define INIVAL_TYPE_UINT 2          ///< Unsigned integer
#define INIVAL_TYPE_LONG 3          ///< Long integer
#define INIVAL_TYPE_ULONG 4         ///< Unsigned long integer
#define INIVAL_TYPE_LLONG 5         ///< Long long integer
#define INIVAL_TYPE_ULLONG 6        ///< Unsigned long long integer
#define INIVAL_TYPE_DOUBLE 7        ///< Double precision float
#define INIVAL_TYPE_FLOAT 8         ///< Single precision float
#define INIVAL_TYPE_STR 9           ///< String
#define INIVAL_TYPE_STR_ARRAY 10    ///< String Array
#define INIVAL_TYPE_BOOL 11         ///< Boolean

#define INIVAL_TO_LIST 1 << 1

/*! \union INIVal
 * \brief Consolidation possible value types
 */
union INIVal {
    int as_int;                      ///< Integer
    unsigned as_uint;                ///< Unsigned integer
    long as_long;                    ///< Long integer
    unsigned long as_ulong;          ///< Unsigned long integer
    long long as_llong;              ///< Long long integer
    unsigned long long as_ullong;    ///< Unsigned long long integer
    double as_double;                ///< Double precision float
    float as_float;                  ///< Single precision float
    char *as_char_p;                 ///< String
    char **as_char_array_p;          ///< String Array
    bool as_bool;                    ///< Boolean
};


/*! \struct INIData
 * \brief A structure to describe an INI data record
 */
struct INIData {
    char *key;                       ///< INI variable name
    char *value;                     ///< INI variable value
};

/*! \struct INISection
 * \brief A structure to describe an INI section
 */
struct INISection {
    size_t data_count;               ///< Total INIData records
    char *key;                       ///< INI section name
    struct INIData **data;           ///< Array of INIData records
};

/*! \struct INIFILE
 * \brief A structure to describe an INI configuration file
 */
struct INIFILE {
    size_t section_count;            ///< Total INISection records
    struct INISection **section;     ///< Array of INISection records
};

/**
 * Open and parse and INI configuration file
 *
 * ~~~.c
 * #include "ini.h"
 * int main(int argc, char *argv[]) {
 *     const char *filename = "example.ini"
 *     struct INIFILE *ini;
 *     ini = ini_open(filename);
 *     if (!ini) {
 *         perror(filename);
 *         exit(1);
 *     }
 * }
 * ~~~
 *
 * @param filename path to INI file
 * @return pointer to INIFILE
 */
struct INIFILE *ini_open(const char *filename);

/**
 * Assign value to a section key
 * @param ini
 * @param type INI_SETVAL_APPEND or INI_SETVAL_REPLACE
 * @param section_name
 * @param key
 * @param value
 * @return
 */
int ini_setval(struct INIFILE **ini, unsigned type, char *section_name, char *key, char *value);

/**
 * Retrieve all data records in an INI section
 *
 * `example.ini`
 * ~~~.ini
 * [example]
 * key_1 = a string
 * key_2 = 100
 * ~~~
 *
 * `example.c`
 * ~~~.c
 * #include "ini.h"
 * int main(int argc, char *argv[]) {
 *     const char *filename = "example.ini"
 *     struct INIData *data;
 *     struct INIFILE *ini;
 *     ini = ini_open(filename);
 *     if (!ini) {
 *         perror(filename);
 *         exit(1);
 *     }
 *     // Read all records in "example" section
 *     for (size_t i = 0; ((data = ini_getall(&ini, "example") != NULL); i++) {
 *         printf("key=%s, value=%s\n", data->key, data->value);
 *     }
 * }
 * ~~~
 *
 * @param ini pointer to INIFILE
 * @param section_name to read
 * @return pointer to INIData
 */
struct INIData *ini_getall(struct INIFILE *ini, char *section_name);

/**
 * Retrieve a single record from a section key
 *
 * `example.ini`
 * ~~~.ini
 * [example]
 * key_1 = a string
 * key_2 = 100
 * ~~~
 *
 * `example.c`
 * ~~~.c
 * #include "ini.h"
 * int main(int argc, char *argv[]) {
 *     const char *filename = "example.ini"
 *     union INIVal *data;
 *     struct INIFILE *ini;
 *     ini = ini_open(filename);
 *     if (!ini) {
 *         perror(filename);
 *         exit(1);
 *     }
 *     data = ini_getval(&ini, "example", "key_1", INIVAL_TYPE_STR);
 *     puts(data.as_char_p);
 *     data = ini_getval(&ini, "example", "key_2", INIVAL_TYPE_INT);
 *     printf("%d\n", data.as_int);
 * }
 * ~~~
 *
 * @param ini pointer to INIFILE
 * @param section_name to read
 * @param key to return
 * @param type INIVAL_TYPE_INT
 * @param type INIVAL_TYPE_UINT
 * @param type INIVAL_TYPE_LONG
 * @param type INIVAL_TYPE_ULONG
 * @param type INIVAL_TYPE_LLONG
 * @param type INIVAL_TYPE_ULLONG
 * @param type INIVAL_TYPE_DOUBLE
 * @param type INIVAL_TYPE_FLOAT
 * @param type INIVAL_TYPE_STR
 * @param type INIVAL_TYPE_STR_ARRAY
 * @param type INIVAL_TYPE_BOOL
 * @param result pointer to INIVal
 * @return 0 on success
 * @return Non-zero on error
 */
int ini_getval(struct INIFILE *ini, char *section_name, char *key, int type, union INIVal *result);

/**
 * Write INIFILE sections and data to a file stream
 * @param ini pointer to INIFILE
 * @param file pointer to address of file stream
 * @return 0 on success, -1 on error
 */
int ini_write(struct INIFILE *ini, FILE **stream, unsigned mode);

/**
 * Free memory allocated by ini_open()
 * @param ini
 */
void ini_free(struct INIFILE **ini);
#endif //OMC_INI_H
