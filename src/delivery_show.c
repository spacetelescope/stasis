#include "delivery.h"

void delivery_debug_show(struct Delivery *ctx) {
    printf("\n====DEBUG====\n");
    printf("%-20s %-10s\n", "System configuration directory:", globals.sysconfdir);
    printf("%-20s %-10s\n", "Mission directory:", ctx->storage.mission_dir);
    printf("%-20s %-10s\n", "Testing enabled:", globals.enable_testing ? "Yes" : "No");
    printf("%-20s %-10s\n", "Docker image builds enabled:", globals.enable_docker ? "Yes" : "No");
    printf("%-20s %-10s\n", "Artifact uploading enabled:", globals.enable_artifactory ? "Yes" : "No");
}

void delivery_meta_show(struct Delivery *ctx) {
    if (globals.verbose) {
        delivery_debug_show(ctx);
    }

    printf("\n====DELIVERY====\n");
    printf("%-20s %-10s\n", "Target Python:", ctx->meta.python);
    printf("%-20s %-10s\n", "Name:", ctx->meta.name);
    printf("%-20s %-10s\n", "Mission:", ctx->meta.mission);
    if (ctx->meta.codename) {
        printf("%-20s %-10s\n", "Codename:", ctx->meta.codename);
    }
    if (ctx->meta.version) {
        printf("%-20s %-10s\n", "Version", ctx->meta.version);
    }
    if (!ctx->meta.final) {
        printf("%-20s %-10d\n", "RC Level:", ctx->meta.rc);
    }
    printf("%-20s %-10s\n", "Final Release:", ctx->meta.final ? "Yes" : "No");
    printf("%-20s %-10s\n", "Based On:", ctx->meta.based_on ? ctx->meta.based_on : "New");
}

void delivery_conda_show(struct Delivery *ctx) {
    printf("\n====CONDA====\n");
    printf("%-20s %-10s\n", "Prefix:", ctx->storage.conda_install_prefix);

    puts("Native Packages:");
    if (strlist_count(ctx->conda.conda_packages) || strlist_count(ctx->conda.conda_packages_defer)) {
        struct StrList *list_conda = strlist_init();
        if (strlist_count(ctx->conda.conda_packages)) {
            strlist_append_strlist(list_conda, ctx->conda.conda_packages);
        }
        if (strlist_count(ctx->conda.conda_packages_defer)) {
            strlist_append_strlist(list_conda, ctx->conda.conda_packages_defer);
        }
        strlist_sort(list_conda, STASIS_SORT_ALPHA);

        for (size_t i = 0; i < strlist_count(list_conda); i++) {
            char *token = strlist_item(list_conda, i);
            if (isempty(token) || isblank(*token) || startswith(token, "-")) {
                continue;
            }
            printf("%21s%s\n", "", token);
        }
        guard_strlist_free(&list_conda);
    } else {
        printf("%21s%s\n", "", "N/A");
    }

    puts("Python Packages:");
    if (strlist_count(ctx->conda.pip_packages) || strlist_count(ctx->conda.pip_packages_defer)) {
        struct StrList *list_python = strlist_init();
        if (strlist_count(ctx->conda.pip_packages)) {
            strlist_append_strlist(list_python, ctx->conda.pip_packages);
        }
        if (strlist_count(ctx->conda.pip_packages_defer)) {
            strlist_append_strlist(list_python, ctx->conda.pip_packages_defer);
        }
        strlist_sort(list_python, STASIS_SORT_ALPHA);

        for (size_t i = 0; i < strlist_count(list_python); i++) {
            char *token = strlist_item(list_python, i);
            if (isempty(token) || isblank(*token) || startswith(token, "-")) {
                continue;
            }
            printf("%21s%s\n", "", token);
        }
        guard_strlist_free(&list_python);
    } else {
        printf("%21s%s\n", "", "N/A");
    }
}

void delivery_tests_show(struct Delivery *ctx) {
    printf("\n====TESTS====\n");
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        if (!ctx->tests[i].name) {
            continue;
        }
        printf("%-20s %-20s %s\n", ctx->tests[i].name,
               ctx->tests[i].version,
               ctx->tests[i].repository);
    }
}

void delivery_runtime_show(struct Delivery *ctx) {
    printf("\n====RUNTIME====\n");
    struct StrList *rt = NULL;
    rt = strlist_copy(ctx->runtime.environ);
    if (!rt) {
        // no data
        return;
    }
    strlist_sort(rt, STASIS_SORT_ALPHA);
    size_t total = strlist_count(rt);
    for (size_t i = 0; i < total; i++) {
        char *item = strlist_item(rt, i);
        if (!item) {
            // not supposed to occur
            msg(STASIS_MSG_WARN | STASIS_MSG_L1, "Encountered unexpected NULL at record %zu of %zu of runtime array.\n", i);
            return;
        }
        printf("%s\n", item);
    }
}

