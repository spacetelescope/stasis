#include "testing.h"



void test_runtime_copy() {
    RuntimeEnv *env = runtime_copy(environ);
    STASIS_ASSERT(env->data != environ, "copied array is not unique");
    int difference = 0;
    for (size_t i = 0; i < strlist_count(env); i++) {
        char *item = strlist_item(env, i);
        if (!strstr_array(environ, item)) {
            difference++;
        }
    }
    STASIS_ASSERT(difference == 0, "copied environment is incomplete");
    runtime_free(env);
}

void test_runtime_copy_empty() {
    char **empty_env = calloc(1, sizeof(empty_env));
    RuntimeEnv *env = runtime_copy(empty_env);
    STASIS_ASSERT(env->num_inuse == 0, "copied array isn't empty");
    GENERIC_ARRAY_FREE(empty_env);
    runtime_free(env);
}

void test_runtime() {
    RuntimeEnv *env = runtime_copy(environ);
    runtime_set(env, "CUSTOM_KEY", "Very custom");
    ssize_t idx;
    STASIS_ASSERT((idx = runtime_contains(env, "CUSTOM_KEY")) >= 0, "CUSTOM_KEY should exist in object");
    STASIS_ASSERT(strcmp(strlist_item(env, idx), "CUSTOM_KEY=Very custom") == 0, "Incorrect index returned by runtime_contains");

    char *custom_value = runtime_get(env, "CUSTOM_KEY");
    STASIS_ASSERT_FATAL(custom_value != NULL, "CUSTOM_KEY should not be NULL");
    STASIS_ASSERT(strcmp(custom_value, "Very custom") == 0, "CUSTOM_KEY has incorrect data");
    STASIS_ASSERT(strstr_array(environ, "CUSTOM_KEY") == NULL, "CUSTOM_KEY should not exist in the global environment");

    runtime_apply(env);
    const char *global_custom_value = getenv("CUSTOM_KEY");
    STASIS_ASSERT_FATAL(global_custom_value != NULL, "CUSTOM_KEY must exist in global environment after calling runtime_apply()");
    STASIS_ASSERT(strcmp(global_custom_value, custom_value) == 0, "local and global CUSTOM_KEY variable are supposed to be identical");

    STASIS_ASSERT(setenv("CUSTOM_KEY", "modified", 1) == 0, "modifying global CUSTOM_KEY failed");
    global_custom_value = getenv("CUSTOM_KEY");
    STASIS_ASSERT(strcmp(global_custom_value, custom_value) != 0, "local and global CUSTOM_KEY variable are supposed to be different");
    guard_free(custom_value);

    runtime_replace(&env, environ);
    runtime_apply(env);

    global_custom_value = getenv("CUSTOM_KEY");
    custom_value = runtime_get(env, "CUSTOM_KEY");
    STASIS_ASSERT_FATAL(custom_value != NULL, "CUSTOM_KEY must exist in local environment after calling runtime_replace()");
    STASIS_ASSERT_FATAL(global_custom_value != NULL, "CUSTOM_KEY must exist in global environment after calling runtime_replace()");
    STASIS_ASSERT(strcmp(global_custom_value, custom_value) == 0, "local and global CUSTOM_KEY variable are supposed to be identical");
    guard_free(custom_value);

    char output_truth[BUFSIZ] = {0};
    char *your_path = runtime_get(env, "PATH");
    sprintf(output_truth, "Your PATH is '%s'.", your_path);
    guard_free(your_path);

    char *output_expanded = runtime_expand_var(env, "Your PATH is '${PATH}'.");
    STASIS_ASSERT(output_expanded != NULL, "expansion of PATH should not fail");
    STASIS_ASSERT(strcmp(output_expanded, output_truth) == 0, "the expansion, and the expected result should be identical");
    guard_free(output_expanded);

    guard_runtime_free(env);
    // TODO: runtime_export()
    // requires dumping stdout to a file and comparing it with the current environment array
}

int main(int argc, char *argv[], char *arge[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_runtime_copy,
        test_runtime_copy_empty,
        test_runtime,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}