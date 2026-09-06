#include <string.h>
#include "ini.h"
#include "regex.h"
#include "parson.h"
#include "ini_validate.h"

const char *type_hint_to_type_string(const int x) {
    switch (x) {
        case INIVAL_TYPE_STR:
            return "INIVAL_TYPE_STR";
        case INIVAL_TYPE_STR_ARRAY:
            return "INIVAL_TYPE_STR_ARRAY";
        case INIVAL_TYPE_BOOL:
            return "INIVAL_TYPE_BOOL";
        case INIVAL_TYPE_UINT:
            return "INIVAL_TYPE_UINT";
        case INIVAL_TYPE_INT:
            return "INIVAL_TYPE_INT";
        case INIVAL_TYPE_CHAR:
            return "INIVAL_TYPE_CHAR";
        case INIVAL_TYPE_SHORT:
            return "INIVAL_TYPE_SHORT";
        case INIVAL_TYPE_UCHAR:
            return "INIVAL_TYPE_UCHAR";
        case INIVAL_TYPE_LONG:
            return "INIVAL_TYPE_LONG";
        case INIVAL_TYPE_ULONG:
            return "INIVAL_TYPE_ULONG";
        case INIVAL_TYPE_LLONG:
            return "INIVAL_TYPE_LLONG";
        case INIVAL_TYPE_ULLONG:
            return "INIVAL_TYPE_ULLONG";
        case INIVAL_TYPE_FLOAT:
            return "INIVAL_TYPE_FLOAT";
        case INIVAL_TYPE_DOUBLE:
            return "INIVAL_TYPE_DOUBLE";
        default:
            return NULL;
    }
}
int type_string_to_type_hint(const char *s) {
    if (strcmp(s, "INIVAL_TYPE_STR") == 0) {
        return INIVAL_TYPE_STR;
    }
    if (strcmp(s, "INIVAL_TYPE_STR_ARRAY") == 0) {
        return INIVAL_TYPE_STR_ARRAY;
    }
    if (strcmp(s, "INIVAL_TYPE_BOOL") == 0) {
        return INIVAL_TYPE_BOOL;
    }
    if (strcmp(s, "INIVAL_TYPE_UINT") == 0) {
        return INIVAL_TYPE_UINT;
    }
    if (strcmp(s, "INIVAL_TYPE_INT") == 0) {
        return INIVAL_TYPE_INT;
    }
    if (strcmp(s, "INIVAL_TYPE_CHAR") == 0) {
        return INIVAL_TYPE_CHAR;
    }
    if (strcmp(s, "INIVAL_TYPE_SHORT") == 0) {
        return INIVAL_TYPE_SHORT;
    }
    if (strcmp(s, "INIVAL_TYPE_UCHAR") == 0) {
        return INIVAL_TYPE_UCHAR;
    }
    if (strcmp(s, "INIVAL_TYPE_LONG") == 0) {
        return INIVAL_TYPE_LONG;
    }
    if (strcmp(s, "INIVAL_TYPE_ULONG") == 0) {
        return INIVAL_TYPE_ULONG;
    }
    if (strcmp(s, "INIVAL_TYPE_LLONG") == 0) {
        return INIVAL_TYPE_LLONG;
    }
    if (strcmp(s, "INIVAL_TYPE_ULLONG") == 0) {
        return INIVAL_TYPE_ULLONG;
    }
    if (strcmp(s, "INIVAL_TYPE_FLOAT") == 0) {
        return INIVAL_TYPE_FLOAT;
    }
    if (strcmp(s, "INIVAL_TYPE_DOUBLE") == 0) {
        return INIVAL_TYPE_DOUBLE;
    }

    return -1;
}

int ini_validate_required_key_exists(struct INIFILE *ini, const char *section_name, char *key) {
    const int status = ini_has_key(ini, section_name, key);
    if (!status) {
        //SYSERROR("%s.%s is required but not defined", section_name, key);
        return 0;
    }
    return 1;
}


int ini_verify_str_regex(const void *data, const char *pattern) {
    regex_t regex;
    int rc = regcomp(&regex, pattern, REG_EXTENDED);
    if (rc != 0) {
        char errbuf[1024] = {0};
        regerror(rc, &regex, errbuf, sizeof(errbuf));
        SYSERROR("regex compilation failed: %s", errbuf);
        regfree(&regex);
        return 1;
    }

    rc = regexec(&regex, data, 0, NULL, 0);

    char errbuf[1024] = {0};
    regerror(rc, &regex, errbuf, sizeof(errbuf));

    if (rc == 0) {
        // Fall through
    } else if (rc == REG_NOMATCH) {
        SYSERROR("%s: regex: '%s', data: '%s'", errbuf, pattern, (const char *) data);
        regfree(&regex);
        return 1;
    } else {
        SYSERROR("regex match failed: %s", errbuf);
        regfree(&regex);
        return 2;
    }
    regfree(&regex);
    return 0;
}

int ini_validate_str_regex(const void *data, const char *pattern, ini_verify_regex_callback *fn[]) {
    int result = 0;
        for (size_t i = 0; fn && fn[i] != NULL; i++) {
            result += fn[i](data, pattern);
        }
    return result;
}

int ini_validate_str_array_regex(const void *data, const char *pattern, ini_verify_regex_callback *fn[]) {
    int result = 0;
    char **arr = split((char *) data, LINE_SEP, 1);
    for (size_t i = 0; arr[i]!= NULL; i++) {
        for (size_t x = 0; fn && fn[x] != NULL; x++) {
            result += fn[x](arr[i], pattern);
        }
    }
    guard_array_free(arr);
    return result;
}

struct StrList *get_required_section_names(const JSON_Array *sections) {
    struct StrList *list = strlist_init();
    if (!list) {
        SYSERROR("unable to allocate memory for required section name list");
        return NULL;
    }
    for (size_t i = 0; i < json_array_get_count(sections); i++) {
        JSON_Object *section = json_array_get_object(sections, i);
        const bool section_required = json_object_get_boolean(section, "required");
        if (section_required) {
            const char *section_name = json_object_get_string(section, "name");
            strlist_append(&list, (char *) section_name);
        }
    }
    return list;
}

struct StrList *get_section_names(const struct INIFILE *ini) {
    struct StrList *list = strlist_init();
    if (!list) {
        SYSERROR("unable to allocate memory for section name list");
        return NULL;
    }

    for (size_t i = 0; i < ini->section_count; i++) {
        const char *name = ini->section[i]->key;
        strlist_append(&list, (char *) name);
    }

    return list;
}

static int status_text_update(char *text, const size_t maxlen, const char *color, const char *s) {
    const size_t remaining = maxlen - strlen(text);
    return snprintf(text + strlen(text), remaining, " %s%s%s ", color, s, STASIS_COLOR_RESET);
}

static void status_text_reset(char *text) {
    text[0] = '\0';
}

int ini_validate_schema_delivery(const char *filename, struct INIFILE *ini, struct tpl_pool **tpl_pool) {
    int errors = 0;
    ini_verify_regex_callback *check_re[] = {
        ini_verify_str_regex,
        NULL,
    };

    JSON_Value *handle = json_parse_file(filename);
    if (!handle) {
        SYSERROR("unable to parse JSON file: %s", filename);
        return -1;
    }

    JSON_Object *root = json_value_get_object(handle);
    if (!root) {
        SYSERROR("unable to get JSON root object");
        json_value_free(handle);
        return -1;
    }

    JSON_Array *sections = json_object_get_array(root, "section");
    if (!sections) {
        SYSERROR("unable to get section array");
        json_value_free(handle);
        return -1;
    }

    struct StrList *required_section_names = get_required_section_names(sections);
    struct StrList *section_names = get_section_names(ini);

    const char *color = STASIS_COLOR_GREEN;
    char status_text[255] = {0};

    SYSINFO("Checking required sections exist...");
    for (size_t i = 0; i < json_array_get_count(sections); i++) {
        const JSON_Object *section = json_array_get_object(sections, i);
        const char *section_name = json_object_get_string(section, "name");
        const bool section_required = json_object_get_boolean(section, "required");
        const char *section_regex = json_object_get_string(section, "regex");
        if (section_required) {
            const int exists = strlist_contains_regex(section_names, section_regex);
            if (!exists) {
                color = STASIS_COLOR_RED;
                SYSERROR("section '%s' ('%s') is required, but missing.", section_name, section_regex);
                status_text_update(status_text, sizeof(status_text), color, "value");
                errors++;
            } else {
                status_text_update(status_text, sizeof(status_text), color, "ok");
            }
        } else {
            status_text_update(status_text, sizeof(status_text), color, "ok");
        }
        SYSINFO("%s [%s]", section_name, status_text);
        status_text_reset(status_text);
        color = STASIS_COLOR_GREEN;
    }

    SYSINFO("Validating section names...");
    for (size_t i = 0; i < json_array_get_count(sections); i++) {
        const JSON_Object *section = json_array_get_object(sections, i);
        const char *section_name = json_object_get_string(section, "name");
        const char *section_regex = json_object_get_string(section, "regex");
        for (size_t j = 0; j < strlist_count(section_names); j++) {
            color = STASIS_COLOR_GREEN;
            const char *item = strlist_item(section_names, j);
            if (startswith(item, section_name)) {
                int state = ini_validate_str_regex(item, section_regex, check_re);
                if (state) {
                    color = STASIS_COLOR_RED;
                    status_text_update(status_text, sizeof(status_text), color, "value");
                } else {
                    status_text_update(status_text, sizeof(status_text), color, "ok");
                }
                SYSINFO("%s [%s]", item, status_text);
                errors += state;
                status_text_reset(status_text);
            }
        }
    }

    SYSINFO("Validating section keys...");
    for (size_t i = 0; i < json_array_get_count(sections); i++) {
        const JSON_Object *section = json_array_get_object(sections, i);
        const char *section_name = json_object_get_string(section, "name");
        for (size_t j = 0; j < ini->section_count; j++) {
            const struct INISection *cur_section = ini->section[j];
            if (!startswith(cur_section->key, section_name)) {
                continue;
            }
            const JSON_Array *section_keys = json_object_get_array(section, "key");
            if (section_keys) {
                for (size_t k = 0; k < json_array_get_count(section_keys); k++) {
                    JSON_Object *key_obj = json_array_get_object(section_keys, k);
                    if (!key_obj) {
                        SYSERROR("unable to get key object");
                        json_value_free(handle);
                        return -1;
                    }
                    const bool key_required = json_object_get_boolean(key_obj, "required");
                    const char *key_expect_type = json_object_get_string(key_obj, "expect_type");
                    if (!key_expect_type) {
                        SYSERROR("unable to get expect_type");
                        json_value_free(handle);
                        return -1;
                    }
                    const char *key_name = json_object_get_string(key_obj, "name");

                    int missing = 0;
                    if (key_required && key_name) {
                        missing = ini_validate_required_key_exists(ini, cur_section->key, (char *) key_name) == 0;
                        if (missing) {
                            status_text_update(status_text, sizeof(status_text), STASIS_COLOR_RED, "required");
                        }
                        errors += missing;
                    }

                    const char *key_regex = json_object_get_string(key_obj, "regex");
                    if (!key_regex) {
                        SYSERROR("unable to get regex pattern");
                        json_value_free(handle);
                        return -1;
                    }


                    int type_hint = type_string_to_type_hint(key_expect_type);
                    if (type_hint != INIVAL_TYPE_STR_ARRAY) {
                        // cast all values to string (for regex matching)
                        type_hint = INIVAL_TYPE_STR;
                    }

                    bool do_all_section_keys = false;
                    union INIVal value;
                    if (key_name) {
                        if (ini_getval(ini, (char *) cur_section->key, (char *) key_name, type_hint, INI_READ_RENDER, &value, tpl_pool)) {
                            status_text_update(status_text, sizeof(status_text), STASIS_COLOR_BLUE, "undefined");
                            SYSINFO("%s.%s [%s]", cur_section->key, key_name, status_text);
                            status_text_reset(status_text);
                            continue;
                        }
                    } else {
                        do_all_section_keys = true;
                    }


                    const char *type_hint_str = type_hint_to_type_string(type_hint);
                    int state = 0;
                    switch (type_hint) {
                        case INIVAL_TYPE_STR: {
                            state = ini_validate_str_regex(value.as_char_p, key_regex, check_re);
                            if (state) {
                                color = STASIS_COLOR_RED;
                                status_text_update(status_text, sizeof(status_text), color, "value");
                            } else {
                                status_text_update(status_text, sizeof(status_text), color, "ok");
                            }
                            SYSINFO("%s.%s [%s]", cur_section->key, key_name, status_text);
                            errors += state;
                            break;
                        }
                        case INIVAL_TYPE_STR_ARRAY:
                            if (!do_all_section_keys) {
                                state = ini_validate_str_array_regex(value.as_char_p, key_regex, check_re);
                                if (state) {
                                    color = STASIS_COLOR_RED;
                                    status_text_update(status_text, sizeof(status_text), color, "value");
                                } else {
                                    status_text_update(status_text, sizeof(status_text), color, "ok");
                                }
                                SYSINFO("%s.%s [%s]", cur_section->key, key_name, status_text);
                                errors += state;
                            } else {
                                struct INIData *data;
                                while ((data = ini_getall(ini, cur_section->key)) != NULL) {
                                    status_text_reset(status_text);
                                    char *declaration = NULL;
                                    if (asprintf(&declaration, "%s=%s", data->key, data->value) < 0) {
                                        SYSERROR("unable to allocate memory for declaration string");
                                        json_value_free(handle);
                                        return -1;
                                    }
                                    state = ini_validate_str_regex(declaration, key_regex, check_re);
                                    if (state) {
                                        color = STASIS_COLOR_RED;
                                        status_text_update(status_text, sizeof(status_text), color, "value");
                                    } else {
                                        status_text_update(status_text, sizeof(status_text), color, "ok");
                                    }
                                    SYSINFO("%s.%s [%s]", cur_section->key, data->key, status_text);
                                    errors += state;
                                    guard_free(declaration);
                                }
                                do_all_section_keys = false;
                            }
                            break;
                        default:
                            SYSWARN("Unhandled type %s:%s, %s", cur_section->key, key_name, type_hint_str);
                    }

                    if (type_hint == INIVAL_TYPE_STR || type_hint == INIVAL_TYPE_STR_ARRAY) {
                        guard_free(value.as_char_p);
                    }
                    status_text_reset(status_text);
                    color = STASIS_COLOR_GREEN;
                }
            }
        }
    }

    guard_strlist_free(&required_section_names);
    guard_strlist_free(&section_names);
    json_value_free(handle);
    return errors;
}