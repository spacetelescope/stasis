/**
 * @file environment.h
 */
#ifndef OMC_ENVIRONMENT_H
#define OMC_ENVIRONMENT_H

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include "environment.h"

typedef struct StrList RuntimeEnv;

ssize_t runtime_contains(RuntimeEnv *env, const char *key);
RuntimeEnv *runtime_copy(char **env);
int runtime_replace(RuntimeEnv **dest, char **src);
char *runtime_get(RuntimeEnv *env, const char *key);
void runtime_set(RuntimeEnv *env, const char *_key, const char *_value);
char *runtime_expand_var(RuntimeEnv *env, char *input);
void runtime_export(RuntimeEnv *env, char **keys);
void runtime_apply(RuntimeEnv *env);
void runtime_free(RuntimeEnv *env);
#endif //OMC_ENVIRONMENT_H
