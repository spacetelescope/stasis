#include "testing.h"
#include "artifactory.h"
#include "delivery.h"

// Import private functions from core
extern int delivery_init_platform(struct Delivery *ctx);
extern int populate_delivery_cfg(struct Delivery *ctx, int render_mode);

struct JFRT_Auth gauth;
struct JFRT_Auth gnoauth;
struct Delivery ctx;
const char *gbuild_name = "test_stasis_jf_build_collect_env";
const char *gbuild_num = "1";

// Delete a build
// For now, I'm keeping these out of the core library.
static int jfrog_cli_rt_build_delete(struct JFRT_Auth *auth, char *build_name, char *build_num) {
    char cmd[STASIS_BUFSIZ];
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd) - 1, "--build \"%s/%s\"", build_name, build_num);
    return jfrog_cli(auth, "rt", "delete", cmd);
}

static int jfrog_cli_rt_delete(struct JFRT_Auth *auth, char *pattern) {
    char cmd[STASIS_BUFSIZ];
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd) - 1, "\"%s\"", pattern);
    return jfrog_cli(auth, "rt", "delete", cmd);
}

void test_jfrog_cli_rt_ping() {
    STASIS_ASSERT_FATAL(jfrog_cli_rt_ping(&gauth) == 0, "server ping failed.");
    STASIS_ASSERT(jfrog_cli_rt_ping(&gnoauth) != 0, "server ping should have failed; auth context is empty.");
}

void test_jfrog_cli_rt_build_collect_publish() {
    struct JFRT_Upload upload;
    jfrt_upload_init(&upload);

    char *filename = "empty_file.txt";
    touch(filename);
    upload.build_name = gbuild_name;
    upload.build_number = gbuild_num;
    STASIS_ASSERT(jfrog_cli_rt_upload(&gauth, &upload, filename, getenv("STASIS_JF_REPO")) == 0, "jf upload failed");
    STASIS_ASSERT(jfrog_cli_rt_build_collect_env(&gauth, gbuild_name, gbuild_num) == 0, "jf environment collection failed");
    STASIS_ASSERT(jfrog_cli_rt_build_publish(&gauth, gbuild_name, gbuild_num) == 0, "jf publish build failed");
    STASIS_ASSERT(jfrog_cli_rt_build_delete(&gauth, gbuild_name, gbuild_num) == 0, "jf delete build failed");
}

void test_jfrog_cli_rt_upload() {
    struct JFRT_Upload upload;
    jfrt_upload_init(&upload);

    char *filename = "empty_file_upload.txt";
    touch(filename);
    STASIS_ASSERT(jfrog_cli_rt_upload(&gauth, &upload, filename, getenv("STASIS_JF_REPO")) == 0, "jf upload failed");
}

void test_jfrog_cli_rt_download() {
    struct JFRT_Download dl;
    memset(&dl, 0, sizeof(dl));

    char *filename = "empty_file_upload.txt";
    char path[PATH_MAX] = {0};
    sprintf(path, "%s/%s", getenv("STASIS_JF_REPO"), filename);
    STASIS_ASSERT(jfrog_cli_rt_download(&gauth, &dl, filename, ".") == 0, "jf download failed");
    STASIS_ASSERT(jfrog_cli_rt_delete(&gauth, path) == 0, "jf delete test artifact failed");
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    memset(&gauth, 0, sizeof(gauth));
    memset(&gnoauth, 0, sizeof(gnoauth));
    memset(&ctx, 0, sizeof(ctx));

    const char *basedir = realpath(".", NULL);
    const char *ws = "workspace";
    mkdir(ws, 0755);
    if (pushd(ws)) {
        SYSERROR("failed to change directory to %s", ws);
        STASIS_ASSERT_FATAL(true, "workspace creation failed");
    }

    // enable messages from the jf tool
    globals.verbose = true;

    // create a limited delivery context
    path_store(&ctx.storage.tools_dir, PATH_MAX, ".", "tools");
    path_store(&ctx.storage.build_dir, PATH_MAX, ".", "build");
    path_store(&ctx.storage.tmpdir, PATH_MAX, ".", "tmp");
    const char *sysconfdir = getenv("STASIS_SYSCONFDIR");
    if (!sysconfdir) {
        sysconfdir = STASIS_SYSCONFDIR;
    }

    char path[PATH_MAX] = {0};
    sprintf(path, "%s/bin:%s", ctx.storage.tools_dir, getenv("PATH"));
    setenv("PATH", path, 1);

    // The default config contains the URL information to download jfrog-cli
    char cfg_path[PATH_MAX] = {0};
    if (strstr(sysconfdir, "..")) {
        sprintf(cfg_path, "%s/%s/stasis.ini", basedir, sysconfdir);
    } else {
        sprintf(cfg_path, "%s/stasis.ini", sysconfdir);
    }
    ctx._stasis_ini_fp.cfg = ini_open(cfg_path);
    if (!ctx._stasis_ini_fp.cfg) {
        SYSERROR("%s: configuration is invalid", cfg_path);
        return STASIS_TEST_SUITE_SKIP;
    }
    delivery_init_platform(&ctx);
    populate_delivery_cfg(&ctx, INI_READ_RENDER);
    delivery_init_artifactory(&ctx);

    // Skip this suite if we're not configured to use it
    if (jfrt_auth_init(&gauth)) {
        SYSERROR("%s", "Not configured to test Artifactory. Skipping.");
        return STASIS_TEST_SUITE_SKIP;
    }

    STASIS_TEST_FUNC *tests[] = {
        test_jfrog_cli_rt_ping,
        test_jfrog_cli_rt_build_collect_publish,
        test_jfrog_cli_rt_upload,
        test_jfrog_cli_rt_download,
    };
    STASIS_TEST_RUN(tests);
    popd();
    STASIS_TEST_END_MAIN();
}