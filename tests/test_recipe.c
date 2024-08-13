#include "testing.h"

static void make_local_recipe(const char *localdir) {
    char path[PATH_MAX] = {0};
    sprintf(path, "./%s", localdir);
    mkdir(path, 0755);
    if (!pushd(path)) {
        touch("meta.yaml");
        touch("build.sh");
        system("git init .");
        system("git config --local user.name test");
        system("git config --local user.email test@test.tld");
        system("git config --local commit.gpgsign false");
        system("git add meta.yaml build.sh");
        system("git commit -m \"initial commit\"");
        popd();
    } else {
        SYSERROR("failed to enter directory: %s (%s)", localdir, strerror(errno));
    }
}

void test_recipe_clone() {
    struct testcase {
        char *recipe_dir;
        char *url;
        char *gitref;
        int expect_type;
        int expect_return;
    };
    struct testcase tc[] = {
        {.recipe_dir = "recipe_condaforge", .url = "https://github.com/conda-forge/fitsverify-feedstock", "HEAD", RECIPE_TYPE_CONDA_FORGE, 0},
        {.recipe_dir = "recipe_astroconda", .url = "https://github.com/astroconda/astroconda-contrib", "HEAD", RECIPE_TYPE_ASTROCONDA, 0},
        {.recipe_dir = "recipe_generic", .url = "local_repo", "HEAD", RECIPE_TYPE_GENERIC, 0},
    };
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        struct testcase *test = &tc[i];
        if (test->recipe_dir && strcmp(test->recipe_dir, "/") != 0 && access(test->recipe_dir, F_OK) == 0) {
            rmtree(test->recipe_dir);
        }
        if (strstr(test->recipe_dir, "_generic")) {
            rmtree("local_repo");
            make_local_recipe("local_repo");
        }

        char *result_path = NULL;
        int result;

        // Clone the repository
        result = recipe_clone(test->recipe_dir, test->url, test->gitref, &result_path);
        // Ensure git didn't return an error
        STASIS_ASSERT(result == test->expect_return, "failed while cloning recipe");
        // Ensure a path to the repository was returned in the result argument
        STASIS_ASSERT(result_path != NULL, "result path should not be NULL");
        STASIS_ASSERT(result_path && access(result_path, F_OK) == 0, "result path should be a valid directory");
        // Verify the repository was detected as the correct recipe type
        STASIS_ASSERT(recipe_get_type(result_path) == test->expect_type, "repository detected as the wrong type");
    }

}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_recipe_clone,
    };
    mkdir("workspace", 0755);
    pushd("workspace");
    STASIS_TEST_RUN(tests);
    popd();
    STASIS_TEST_END_MAIN();
}