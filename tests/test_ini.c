#include "testing.h"
#include "ini.h"

void test_ini_open_empty() {
    struct INIFILE *ini;
    ini = ini_open("/dev/null");
    OMC_ASSERT(ini->section_count == 1, "should have at least one section pre-allocated");
    OMC_ASSERT(ini->section != NULL, "default section not present");
    ini_free(&ini);
    OMC_ASSERT(ini == NULL, "ini_free did not set pointer argument to NULL");
}

void test_ini_open() {
    const char *filename = "ini_open.ini";
    const char *data = "[default]\na=1\nb=2\nc=3\n[section name]test=true\n";
    struct INIFILE *ini;
    omc_testing_write_ascii(filename, data);
    ini = ini_open(filename);
    OMC_ASSERT(ini->section_count == 2, "should have two sections");
    OMC_ASSERT(ini->section != NULL, "sections are not allocated");
    ini_free(&ini);
    OMC_ASSERT(ini == NULL, "ini_free did not set pointer argument to NULL");
    remove(filename);
}

void test_ini_section_search() {
    const char *filename = "ini_open.ini";
    const char *data = "[default]\na=1\nb=2\nc=3\n[section name here]\ntest=true\n";
    struct INIFILE *ini;
    struct INISection *section;

    omc_testing_write_ascii(filename, data);

    ini = ini_open(filename);
    section = ini_section_search(&ini, INI_SEARCH_BEGINS, "section");
    OMC_ASSERT(section->data_count == 1, "should have one data variable");
    OMC_ASSERT(strcmp(section->data[0]->key, "test") == 0, "should have one data variable named 'test'");
    OMC_ASSERT(strcmp(section->data[0]->value, "true") == 0, "'test' should be equal to 'true'");
    OMC_ASSERT(strcmp(section->key, "section name here") == 0, "defined section not found");
    section = ini_section_search(&ini, INI_SEARCH_SUBSTR, "name");
    OMC_ASSERT(strcmp(section->data[0]->key, "test") == 0, "should have one data variable named 'test'");
    OMC_ASSERT(strcmp(section->data[0]->value, "true") == 0, "'test' should be equal to 'true'");
    OMC_ASSERT(strcmp(section->key, "section name here") == 0, "defined section not found");
    section = ini_section_search(&ini, INI_SEARCH_EXACT, "section name here");
    OMC_ASSERT(strcmp(section->data[0]->key, "test") == 0, "should have one data variable named 'test'");
    OMC_ASSERT(strcmp(section->data[0]->value, "true") == 0, "'test' should be equal to 'true'");
    OMC_ASSERT(strcmp(section->key, "section name here") == 0, "defined section not found");

    section = ini_section_search(&ini, INI_SEARCH_BEGINS, "bogus");
    OMC_ASSERT(section == NULL, "bogus section search returned non-NULL");
    section = ini_section_search(&ini, INI_SEARCH_BEGINS, NULL);
    OMC_ASSERT(section == NULL, "NULL argument returned non-NULL");
    ini_free(&ini);
    remove(filename);
}

void test_ini_has_key() {
    const char *filename = "ini_open.ini";
    const char *data = "[default]\na=1\nb=2\nc=3\n[section name here]\ntest=true\n";
    struct INIFILE *ini;

    omc_testing_write_ascii(filename, data);

    ini = ini_open(filename);
    OMC_ASSERT(ini_has_key(ini, "default", "a") == true, "'default:a' key should exist");
    OMC_ASSERT(ini_has_key(NULL, "default", NULL) == false, "NULL ini object should return false");
    OMC_ASSERT(ini_has_key(ini, "default", NULL) == false, "NULL key should return false");
    OMC_ASSERT(ini_has_key(ini, NULL, "a") == false, "NULL section should return false");
    OMC_ASSERT(ini_has_key(ini, "default", "noname") == false, "'default:noname' key should not exist");
    OMC_ASSERT(ini_has_key(ini, "doesnotexist", "name") == false, "'doesnotexist:name' key should not exist");
    ini_free(&ini);
    remove(filename);
}

void test_ini_setval_getval() {
    const char *filename = "ini_open.ini";
    const char *data = "[default]\na=1\nb=2\nc=3\n[section name here]\ntest=true\n";
    struct INIFILE *ini;

    omc_testing_write_ascii(filename, data);

    ini = ini_open(filename);
    union INIVal val;
    OMC_ASSERT(ini_setval(&ini, INI_SETVAL_REPLACE, "default", "a", "changed") == 0, "failed to set value");
    OMC_ASSERT(ini_getval(ini, "default", "a", INIVAL_TYPE_STR, &val) == 0, "failed to get value");
    OMC_ASSERT(strcmp(val.as_char_p, "a") != 0, "unexpected value loaded from modified variable");
    OMC_ASSERT(strcmp(val.as_char_p, "changed") == 0, "unexpected value loaded from modified variable");

    OMC_ASSERT(ini_setval(&ini, INI_SETVAL_APPEND, "default", "a", " twice") == 0, "failed to set value");
    OMC_ASSERT(ini_getval(ini, "default", "a", INIVAL_TYPE_STR, &val) == 0, "failed to get value");
    OMC_ASSERT(strcmp(val.as_char_p, "changed") != 0, "unexpected value loaded from modified variable");
    OMC_ASSERT(strcmp(val.as_char_p, "changed twice") == 0, "unexpected value loaded from modified variable");
    ini_free(&ini);
    remove(filename);
}

int main(int argc, char *argv[]) {
    OMC_TEST_BEGIN_MAIN();
    OMC_TEST_FUNC *tests[] = {
        test_ini_open_empty,
        test_ini_open,
        test_ini_section_search,
        test_ini_has_key,
        test_ini_setval_getval,
    };
    OMC_TEST_RUN(tests);
    OMC_TEST_END_MAIN();
}