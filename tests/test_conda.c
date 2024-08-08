#include "testing.h"

char cwd_start[PATH_MAX];
char cwd_workspace[PATH_MAX];
int conda_is_installed = 0;

void test_micromamba() {
    char mm_prefix[PATH_MAX] = {0};
    char c_prefix[PATH_MAX] = {0};
    snprintf(mm_prefix, strlen(cwd_workspace) + strlen("micromamba") + 2, "%s/%s", cwd_workspace, "micromamba");
    snprintf(c_prefix, strlen(mm_prefix) + strlen("conda") + 2, "%s/%s", mm_prefix, "conda");

    struct testcase {
        struct MicromambaInfo mminfo;
        const char *cmd;
        int result;
    };
    struct testcase tc[] = {
            {.mminfo = {.micromamba_prefix = mm_prefix, .conda_prefix = c_prefix}, .cmd = "info", .result = 0},
            {.mminfo = {.micromamba_prefix = mm_prefix, .conda_prefix = c_prefix}, .cmd = "env list", .result = 0},
            {.mminfo = {.micromamba_prefix = mm_prefix, .conda_prefix = c_prefix}, .cmd = "run python -V", .result = 0},
            {.mminfo = {.micromamba_prefix = mm_prefix, .conda_prefix = c_prefix}, .cmd = "no_such_option", .result = 109},
    };

    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        struct testcase *item = &tc[i];
        int result = micromamba(&item->mminfo, (char *) item->cmd);
        if (result > 0) {
            result = result >> 8;
        }
        STASIS_ASSERT(result == item->result, "unexpected exit value");
        SYSERROR("micromamba command: '%s' (returned: %d)", item->cmd, result);
    }
}

static char conda_prefix[PATH_MAX] = {0};
struct Delivery ctx;

void test_conda_installation() {
    char *install_url = calloc(255, sizeof(install_url));
    delivery_get_installer_url(&ctx, install_url);
    delivery_get_installer(&ctx, install_url);
    delivery_install_conda(ctx.conda.installer_path, ctx.storage.conda_install_prefix);
    STASIS_ASSERT_FATAL(access(ctx.storage.conda_install_prefix, F_OK) == 0, "conda was not installed correctly");
    STASIS_ASSERT_FATAL(conda_activate(ctx.storage.conda_install_prefix, "base") == 0, "unable to activate base environment");
    STASIS_ASSERT_FATAL(conda_exec("info") == 0, "conda was not installed correctly");
    conda_is_installed = 1;
}

void test_conda_activate() {
    struct testcase {
        const char *key;
        char *value;
    };
    struct testcase tc[] = {
        {.key = "CONDA_DEFAULT_ENV",  .value = "base"},
        {.key = "CONDA_SHLVL", .value = "1"},
        {.key = "_CE_CONDA", ""},
        {.key = "_CE_M", ""},
        {.key = "CONDA_PREFIX", ctx.storage.conda_install_prefix},
    };

    STASIS_SKIP_IF(!conda_is_installed, "cannot run without conda");
    STASIS_ASSERT_FATAL(conda_activate(ctx.storage.conda_install_prefix, "base") == 0, "unable to activate base environment");

    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        struct testcase *item = &tc[i];
        char *value = getenv(item->key);
        STASIS_ASSERT(value != NULL, "expected key to exist, but doesn't. conda is not activated.");
        if (value) {
            STASIS_ASSERT(strcmp(value, item->value) == 0, "conda variable value mismatch");
        }
    }
}

void test_conda_exec() {
    STASIS_ASSERT(conda_exec("--version") == 0, "conda is broken");
    STASIS_ASSERT(conda_exec("info") == 0, "conda is broken");
    STASIS_ASSERT(conda_exec("search fitsverify") == 0, "conda is broken");
    STASIS_ASSERT(conda_exec("create -n testenv") == 0, "conda is broken");
    STASIS_ASSERT(conda_exec("install -n testenv fitsverify") == 0, "conda is broken");
    STASIS_ASSERT(conda_exec("update -n testenv fitsverify") == 0, "conda is broken");
    STASIS_ASSERT(conda_exec("list -n testenv") == 0, "conda is broken");
    STASIS_ASSERT(conda_exec("run -n testenv fitsverify") == 0, "conda is broken");
    STASIS_ASSERT(conda_exec("notasubcommand --help") != 0, "conda is broken");
}

void test_python_exec() {
    const char *python_system_path = find_program("python3");
    char python_path[PATH_MAX];
    sprintf(python_path, "%s/bin/python3", ctx.storage.conda_install_prefix);

    STASIS_ASSERT(strcmp(python_path, python_system_path ? python_system_path : "/not/found") == 0, "conda is not configured correctly.");
    STASIS_ASSERT(python_exec("-V") == 0, "python is broken");
}

void test_conda_setup_headless() {
    globals.conda_packages = strlist_init();
    globals.pip_packages = strlist_init();
    strlist_append(&globals.conda_packages, "boa");
    strlist_append(&globals.conda_packages, "conda-build");
    strlist_append(&globals.conda_packages, "conda-verify");
    strlist_append(&globals.pip_packages, "pytest");
    STASIS_ASSERT(conda_setup_headless() == 0, "headless configuration failed");
}

void test_conda_env_create_from_uri() {
    const char *url = "https://ssb.stsci.edu/jhunk/stasis_test/test_conda_env_create_from_uri.yml";
    char *name = strdup(__FUNCTION__);
    STASIS_ASSERT(conda_env_create_from_uri(name, (char *) url) == 0, "creating an environment from a remote source failed");
    free(name);
}

void test_conda_env_create_export_remove() {
    char *name = strdup(__FUNCTION__);
    STASIS_ASSERT(conda_env_create(name, "3", "fitsverify") == 0, "unable to create a simple environment");
    STASIS_ASSERT(conda_env_export(name, ".", name) == 0, "unable to export an environment");
    STASIS_ASSERT(conda_env_remove(name) == 0, "unable to remove an environment");
    free(name);
}

void test_conda_index() {
    mkdirs("channel/noarch", 0755);
    STASIS_ASSERT(conda_index("channel") == 0, "cannot index a simple conda channel");
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_micromamba,
        test_conda_installation,
        test_conda_activate,
        test_conda_setup_headless,
        test_conda_exec,
        test_python_exec,
        test_conda_env_create_from_uri,
        test_conda_env_create_export_remove,
        test_conda_index,
    };

    const char *ws = "workspace";
    getcwd(cwd_start, sizeof(cwd_start) - 1);
    mkdir(ws, 0755);
    chdir(ws);
    getcwd(cwd_workspace, sizeof(cwd_workspace) - 1);

    snprintf(conda_prefix, strlen(cwd_workspace) + strlen("conda") + 2, "%s/conda", cwd_workspace);

    const char *mockinidata = "[meta]\n"
                              "name = mock\n"
                              "version = 1.0.0\n"
                              "rc = 1\n"
                              "mission = generic\n"
                              "python = 3.11\n"
                              "[conda]\n"
                              "installer_name = Miniforge3\n"
                              "installer_version = 24.3.0-0\n"
                              "installer_platform = {{env:STASIS_CONDA_PLATFORM}}\n"
                              "installer_arch = {{env:STASIS_CONDA_ARCH}}\n"
                              "installer_baseurl = https://github.com/conda-forge/miniforge/releases/download/24.3.0-0\n";
    stasis_testing_write_ascii("mock.ini", mockinidata);
    struct INIFILE *ini = ini_open("mock.ini");
    ctx._stasis_ini_fp.delivery = ini;
    ctx._stasis_ini_fp.delivery_path = realpath("mock.ini", NULL);

    setenv("TMPDIR", cwd_workspace, 1);
    globals.sysconfdir = getenv("STASIS_SYSCONFDIR");
    ctx.storage.root = strdup(cwd_workspace);

    setenv("LANG", "C", 1);
    bootstrap_build_info(&ctx);
    delivery_init(&ctx, INI_READ_RENDER);

    STASIS_TEST_RUN(tests);

    chdir(cwd_start);
    if (rmtree(cwd_workspace)) {
        perror(cwd_workspace);
    }
    STASIS_TEST_END_MAIN();
}