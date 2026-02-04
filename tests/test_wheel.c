#include "delivery.h"
#include "testing.h"
#include "str.h"
#include "wheel.h"

char cwd_start[PATH_MAX];
char cwd_workspace[PATH_MAX];
int conda_is_installed = 0;
static char conda_prefix[PATH_MAX] = {0};
struct Delivery ctx;
static const char *testpkg_filename = "testpkg/dist/testpkg-1.0.0-py3-none-any.whl";


static void test_wheel_package() {
    const char *filename = testpkg_filename;
    struct Wheel *wheel = NULL;
    wheel_package(&wheel, filename);
    STASIS_ASSERT(wheel != NULL, "wheel cannot be NULL");
    STASIS_ASSERT(wheel != NULL, "wheel_package failed to initialize wheel struct");
    STASIS_ASSERT(wheel->record != NULL, "Record cannot be NULL");
    STASIS_ASSERT(wheel->num_record > 0, "Record count cannot be zero");
    STASIS_ASSERT(wheel->tag != NULL, "Package tag list cannot be NULL");
    STASIS_ASSERT(wheel->generator != NULL, "Generator field cannot be NULL");
    STASIS_ASSERT(wheel->top_level != NULL, "Top level directory name cannot be NULL");
    STASIS_ASSERT(wheel->wheel_version != NULL, "Wheel version cannot be NULL");
    STASIS_ASSERT(wheel->metadata != NULL, "Metadata cannot be NULL");
    STASIS_ASSERT(wheel->metadata->name != NULL, "Metadata::name cannot be NULL");
    STASIS_ASSERT(wheel->metadata->version != NULL, "Metadata::version cannot be NULL");
    STASIS_ASSERT(wheel->metadata->metadata_version != NULL, "Metadata::version (of metadata) cannot be NULL");
    STASIS_ASSERT(wheel_show_info(wheel) == 0, "wheel_show_info should never fail. Enum(s) might be out of sync with META_*_KEYS array(s)");
    wheel_package_free(&wheel);
    STASIS_ASSERT(wheel == NULL, "wheel struct should be NULL after free");
}

static void mock_python_package() {
    const char *pyproject_toml_data = "[build-system]\n"
        "requires = [\"setuptools >= 77.0.3\"]\n"
        "build-backend = \"setuptools.build_meta\"\n"
        "\n"
        "[project]\n"
        "name = \"testpkg\"\n"
        "version = \"1.0.0\"\n"
        "authors = [{name = \"STASIS Team\", email = \"stasis@not-a-real-domain.tld\"}]\n"
        "description = \"A STASIS test package\"\n"
        "readme = \"README.md\"\n"
        "license = \"BSD-3-Clause\"\n"
        "classifiers = [\"Programming Language :: Python :: 3\"]\n"
        "\n"
        "[project.urls]\n"
        "Homepage = \"https://not-a-real-address.tld\"\n"
        "Documentation = \"https://not-a-real-address.tld/docs\"\n"
        "Repository = \"https://not-a-real-address.tld/repo.git\"\n"
        "Issues = \"https://not-a-real-address.tld/tracker\"\n"
        "Changelog = \"https://not-a-real-address.tld/changes\"\n";
    const char *readme = "# testpkg\n\nThis is a test package, for testing.\n";

    mkdir("testpkg", 0755);
    mkdir("testpkg/src", 0755);
    mkdir("testpkg/src/testpkg", 0755);
    if (touch("testpkg/src/testpkg/__init__.py")) {
        fprintf(stderr, "unable to write __init__.py");
        exit(1);
    }
    if (touch("testpkg/README.md")) {
        fprintf(stderr, "unable to write README.md");
        exit(1);
    }
    if (stasis_testing_write_ascii("testpkg/pyproject.toml", pyproject_toml_data)) {
        perror("unable to write pyproject.toml");
        exit(1);
    }
    if (stasis_testing_write_ascii("testpkg/README.md", readme)) {
        perror("unable to write readme");
        exit(1);
    }
    if (pip_exec("install build")) {
        fprintf(stderr, "unable to install build tool using pip\n");
        exit(1);
    }
    if (python_exec("-m build -w ./testpkg")) {
        fprintf(stderr, "unable build test package");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_wheel_package,
    };

    char ws[] = "workspace_XXXXXX";
    if (!mkdtemp(ws)) {
        perror("mkdtemp");
        exit(1);
    }
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

    const char *sysconfdir = getenv("STASIS_SYSCONFDIR");
    globals.sysconfdir = strdup(sysconfdir ? sysconfdir : STASIS_SYSCONFDIR);
    ctx.storage.root = strdup(cwd_workspace);
    char *cfgfile = join((char *[]) {globals.sysconfdir, "stasis.ini", NULL}, "/");
    if (!cfgfile) {
        perror("unable to create path to global config");
        exit(1);
    }

    ctx._stasis_ini_fp.cfg = ini_open(cfgfile);
    if (!ctx._stasis_ini_fp.cfg) {
        fprintf(stderr, "unable to open config file, %s\n", cfgfile);
        exit(1);
    }
    ctx._stasis_ini_fp.cfg_path = realpath(cfgfile, NULL);
    if (!ctx._stasis_ini_fp.cfg_path) {
        fprintf(stderr, "unable to determine absolute path of config, %s\n", cfgfile);
        exit(1);
    }
    guard_free(cfgfile);

    setenv("LANG", "C", 1);
    if (bootstrap_build_info(&ctx)) {
        fprintf(stderr, "bootstrap_build_info failed\n");
        exit(1);
    }
    if (delivery_init(&ctx, INI_READ_RENDER)) {
        fprintf(stderr, "delivery_init failed\n");
        exit(1);
    }

    char *install_url = calloc(255, sizeof(install_url));
    delivery_get_conda_installer_url(&ctx, install_url);
    delivery_get_conda_installer(&ctx, install_url);
    delivery_install_conda(ctx.conda.installer_path, ctx.storage.conda_install_prefix);
    guard_free(install_url);

    if (conda_activate(ctx.storage.conda_install_prefix, "base")) {
        fprintf(stderr, "conda_activate failed\n");
        exit(1);
    }
    if (conda_exec("install -y boa conda-build")) {
        fprintf(stderr, "conda_exec failed\n");
        exit(1);
    }
    if (conda_setup_headless()) {
        fprintf(stderr, "conda_setup_headless failed\n");
        exit(1);
    }
    if (conda_env_create("testpkg", ctx.meta.python, NULL)) {
        fprintf(stderr, "conda_env_create failed\n");
        exit(1);
    }
    if (conda_activate(ctx.storage.conda_install_prefix, "testpkg")) {
        fprintf(stderr, "conda_activate failed\n");
        exit(1);
    }

    mock_python_package();

    STASIS_TEST_RUN(tests);

    if (chdir(cwd_start) < 0) {
        fprintf(stderr, "chdir failed\n");
        exit(1);
    }
    if (rmtree(cwd_workspace)) {
        perror(cwd_workspace);
    }
    delivery_free(&ctx);
    globals_free();

    STASIS_TEST_END_MAIN();

}