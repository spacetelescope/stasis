#include "testing.h"
#include "ini.h"

void test_ini_open_empty() {
    struct INIFILE *ini;
    ini = ini_open("/dev/null");
    STASIS_ASSERT(ini->section_count == 1, "should have at least one section pre-allocated");
    STASIS_ASSERT(ini->section != NULL, "default section not present");
    ini_free(&ini);
    STASIS_ASSERT(ini == NULL, "ini_free did not set pointer argument to NULL");
}

void test_ini_open() {
    const char *filename = "ini_open.ini";
    const char *data = "[default]\na=1\nb=2\nc=3\n[section name]test=true\n";
    struct INIFILE *ini;
    stasis_testing_write_ascii(filename, data);
    ini = ini_open(filename);
    STASIS_ASSERT(ini->section_count == 2, "should have two sections");
    STASIS_ASSERT(ini->section != NULL, "sections are not allocated");
    ini_free(&ini);
    STASIS_ASSERT(ini == NULL, "ini_free did not set pointer argument to NULL");
    remove(filename);
}

void test_ini_section_search() {
    const char *filename = "ini_open.ini";
    const char *data = "[default]\na=1\nb=2\nc=3\n[section name here]\ntest=true\n";
    struct INIFILE *ini;
    struct INISection *section;

    stasis_testing_write_ascii(filename, data);

    ini = ini_open(filename);
    section = ini_section_search(&ini, INI_SEARCH_BEGINS, "section");
    STASIS_ASSERT(section->data_count == 1, "should have one data variable");
    STASIS_ASSERT(strcmp(section->data[0]->key, "test") == 0, "should have one data variable named 'test'");
    STASIS_ASSERT(strcmp(section->data[0]->value, "true") == 0, "'test' should be equal to 'true'");
    STASIS_ASSERT(strcmp(section->key, "section name here") == 0, "defined section not found");
    section = ini_section_search(&ini, INI_SEARCH_SUBSTR, "name");
    STASIS_ASSERT(strcmp(section->data[0]->key, "test") == 0, "should have one data variable named 'test'");
    STASIS_ASSERT(strcmp(section->data[0]->value, "true") == 0, "'test' should be equal to 'true'");
    STASIS_ASSERT(strcmp(section->key, "section name here") == 0, "defined section not found");
    section = ini_section_search(&ini, INI_SEARCH_EXACT, "section name here");
    STASIS_ASSERT(strcmp(section->data[0]->key, "test") == 0, "should have one data variable named 'test'");
    STASIS_ASSERT(strcmp(section->data[0]->value, "true") == 0, "'test' should be equal to 'true'");
    STASIS_ASSERT(strcmp(section->key, "section name here") == 0, "defined section not found");

    section = ini_section_search(&ini, INI_SEARCH_BEGINS, "bogus");
    STASIS_ASSERT(section == NULL, "bogus section search returned non-NULL");
    section = ini_section_search(&ini, INI_SEARCH_BEGINS, NULL);
    STASIS_ASSERT(section == NULL, "NULL argument returned non-NULL");
    ini_free(&ini);
    remove(filename);
}

void test_ini_has_key() {
    const char *filename = "ini_open.ini";
    const char *data = "[default]\na=1\nb=2\nc=3\n[section name here]\ntest=true\n";
    struct INIFILE *ini;

    stasis_testing_write_ascii(filename, data);

    ini = ini_open(filename);
    STASIS_ASSERT(ini_has_key(ini, "default", "a") == true, "'default:a' key should exist");
    STASIS_ASSERT(ini_has_key(NULL, "default", NULL) == false, "NULL ini object should return false");
    STASIS_ASSERT(ini_has_key(ini, "default", NULL) == false, "NULL key should return false");
    STASIS_ASSERT(ini_has_key(ini, NULL, "a") == false, "NULL section should return false");
    STASIS_ASSERT(ini_has_key(ini, "default", "noname") == false, "'default:noname' key should not exist");
    STASIS_ASSERT(ini_has_key(ini, "doesnotexist", "name") == false, "'doesnotexist:name' key should not exist");
    ini_free(&ini);
    remove(filename);
}

void test_ini_setval_getval() {
    int render_mode = INI_READ_RENDER;
    const char *filename = "ini_open.ini";
    const char *data = "[default]\na=1\nb=2\nc=3\n[section name here]\ntest=true\n";
    struct INIFILE *ini;

    stasis_testing_write_ascii(filename, data);

    ini = ini_open(filename);
    union INIVal val;
    STASIS_ASSERT(ini_setval(&ini, INI_SETVAL_REPLACE, "default", "a", "changed") == 0, "failed to set value");
    STASIS_ASSERT(ini_getval(ini, "default", "a", INIVAL_TYPE_STR, render_mode, &val) == 0, "failed to get value");
    STASIS_ASSERT(strcmp(val.as_char_p, "a") != 0, "unexpected value loaded from modified variable");
    STASIS_ASSERT(strcmp(val.as_char_p, "changed") == 0, "unexpected value loaded from modified variable");

    STASIS_ASSERT(ini_setval(&ini, INI_SETVAL_APPEND, "default", "a", " twice") == 0, "failed to set value");
    STASIS_ASSERT(ini_getval(ini, "default", "a", INIVAL_TYPE_STR, render_mode, &val) == 0, "failed to get value");
    STASIS_ASSERT(strcmp(val.as_char_p, "changed") != 0, "unexpected value loaded from modified variable");
    STASIS_ASSERT(strcmp(val.as_char_p, "changed twice") == 0, "unexpected value loaded from modified variable");
    ini_free(&ini);
    remove(filename);
}

void test_ini_getval_wrappers() {
    int render_mode = INI_READ_RENDER;
    const char *filename = "ini_open.ini";
    const char *data = "[default]\n"
                       "char_01a=-1\n"
                       "char_01b=127\n"
                       "uchar_01a=0\n"
                       "uchar_01b=255\n"
                       "short_01a=-1\n"
                       "short_01b=1\n"
                       "ushort_01a=0\n"
                       "ushort_01b=1\n"
                       "int_01a=-1\n"
                       "int_01b=1\n"
                       "uint_01a=0\n"
                       "uint_01b=1\n"
                       "long_01a=-1\n"
                       "long_01b=1\n"
                       "ulong_01a=0\n"
                       "ulong_01b=1\n"
                       "llong_01a=-1\n"
                       "llong_01b=1\n"
                       "ullong_01a=0\n"
                       "ullong_01b=1\n"
                       "float_01a=-1.5\n"
                       "float_01b=1.5\n"
                       "double_01a=-1.5\n"
                       "double_01b=1.5\n";
    struct INIFILE *ini = NULL;
    int err = 0;

    stasis_testing_write_ascii(filename, data);
    ini = ini_open(filename);
    STASIS_ASSERT_FATAL(ini != NULL, "could not open test data");

    STASIS_ASSERT(ini_getval_char(ini, "default", "char_01a", render_mode, &err) == -1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: char");
    STASIS_ASSERT(ini_getval_char(ini, "default", "char_01b", render_mode, &err) == 127, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: char");

    STASIS_ASSERT(ini_getval_uchar(ini, "default", "uchar_01a", render_mode, &err) == 0, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: uchar");
    STASIS_ASSERT(ini_getval_uchar(ini, "default", "uchar_01b", render_mode, &err) == 255, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: uchar");

    STASIS_ASSERT(ini_getval_short(ini, "default", "short_01a", render_mode, &err) == -1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: short");
    STASIS_ASSERT(ini_getval_short(ini, "default", "short_01b", render_mode, &err) == 1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: short");

    STASIS_ASSERT(ini_getval_ushort(ini, "default", "ushort_01a", render_mode, &err) == 0, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: ushort");
    STASIS_ASSERT(ini_getval_ushort(ini, "default", "ushort_01b", render_mode, &err) == 1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: ushort");

    STASIS_ASSERT(ini_getval_int(ini, "default", "int_01a", render_mode, &err) == -1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: int");
    STASIS_ASSERT(ini_getval_int(ini, "default", "int_01b", render_mode, &err) == 1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: int");

    STASIS_ASSERT(ini_getval_uint(ini, "default", "uint_01a", render_mode, &err) == 0, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: uint");
    STASIS_ASSERT(ini_getval_uint(ini, "default", "uint_01b", render_mode, &err) == 1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: uint");

    STASIS_ASSERT(ini_getval_long(ini, "default", "long_01a", render_mode, &err) == -1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: long");
    STASIS_ASSERT(ini_getval_long(ini, "default", "long_01b", render_mode, &err) == 1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: long");

    STASIS_ASSERT(ini_getval_ulong(ini, "default", "ulong_01a", render_mode, &err) == 0, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: ulong");
    STASIS_ASSERT(ini_getval_ulong(ini, "default", "ulong_01b", render_mode, &err) == 1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: ulong");

    STASIS_ASSERT(ini_getval_llong(ini, "default", "llong_01a", render_mode, &err) == -1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: llong");
    STASIS_ASSERT(ini_getval_llong(ini, "default", "llong_01b", render_mode, &err) == 1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: llong");

    STASIS_ASSERT(ini_getval_ullong(ini, "default", "ullong_01a", render_mode, &err) == 0, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: ullong");
    STASIS_ASSERT(ini_getval_ullong(ini, "default", "ullong_01b", render_mode, &err) == 1, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: ullong");

    STASIS_ASSERT(ini_getval_float(ini, "default", "float_01a", render_mode, &err) == -1.5F, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: float");
    STASIS_ASSERT(ini_getval_float(ini, "default", "float_01b", render_mode, &err) == 1.5F, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: float");

    STASIS_ASSERT(ini_getval_double(ini, "default", "double_01a", render_mode, &err) == -1.5L, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: double");
    STASIS_ASSERT(ini_getval_double(ini, "default", "double_01b", render_mode, &err) == 1.5L, "returned unexpected value");
    STASIS_ASSERT(err == 0, "failed to convert type: double");

    ini_free(&ini);
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_ini_open_empty,
        test_ini_open,
        test_ini_section_search,
        test_ini_has_key,
        test_ini_setval_getval,
        test_ini_getval_wrappers,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}