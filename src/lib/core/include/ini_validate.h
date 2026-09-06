#ifndef STASIS_INI_VALIDATE_H
#define STASIS_INI_VALIDATE_H

#include "ini.h"
#include "template.h"
#define STASIS_VALIDATION_SCHEMA_DELIVERY "schema/delivery.json"

typedef int (ini_verify_callback)(const char *, const char *, const void *);
typedef int (ini_verify_regex_callback)(const void *, const char *);

int ini_validate_schema_delivery(const char *, struct INIFILE *, struct tpl_pool **);

int ini_validate_required_key_exists(struct INIFILE *ini, const char *section_name, char *key);

int ini_verify_bool(const char *origin, const char *key, const void *val);
int ini_verify_int(const char *origin, const char *key, const void *val);
int ini_verify_str_no_spaces(const char *origin, const char *key, const void *val);
int ini_verify_str_not_empty(const char *origin, const char *key, const void *val);
int ini_verify_str_is_printable(const char *origin, const char *key, const void *val);
int ini_verify_str_no_control_characters(const char *origin, const char *key, const void *val);
int ini_verify_str_no_version_spec_characters(const char *origin, const char *key, const void *val);
int ini_verify_str_no_invalid_codename_characters(const char *origin, const char *key, const void *val);

int ini_validate__ruleset_str_baseline(const char *origin, const char *key, const void *val);
int ini_validate__ruleset_strlist_baseline(const char *origin, const char *key, const void *val);
int ini_validate__ruleset_strlist_as_runtime(const char *origin, const char *key, const void *val);
int ini_validate__ruleset_strlist_section_as_runtime(const char *origin, const char *key, const void *val);
int ini_validate__ruleset_str_as_path(const char *origin, const char *key, const void *val);
int ini_validate__ruleset_strlist_as_path(const char *origin, const char *key, const void *val);

int ini_validate_str(struct INIFILE *ini, const char *section_name, const char *key, ini_verify_callback *fn[]);
int ini_validate_strlist(struct INIFILE *ini, const char *section_name, const char *key, ini_verify_callback *fn[]);
int ini_validate_strlist_section(struct INIFILE *ini, const char *section_name, const char *key, ini_verify_callback *fn[]);
int ini_validate_bool(struct INIFILE *ini, const char *section_name, const char *key, ini_verify_callback *fn[]);
int ini_validate_int(struct INIFILE *ini, const char *section_name, const char *key, ini_verify_callback *fn[]);


#endif  // STASIS_INI_VALIDATE_H