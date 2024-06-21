#include "testing.h"
struct DockerCapabilities cap_suite;

void test_docker_capable() {
    struct DockerCapabilities cap;
    int result = docker_capable(&cap);
    STASIS_ASSERT(result == true, "docker installation unusable");
    STASIS_ASSERT(cap.available, "docker is not available");
    STASIS_ASSERT(cap.build != 0, "docker cannot build images");
    // no cap.podman assertion. doesn't matter if we have docker or podman
    STASIS_ASSERT(cap.usable, "docker is unusable");
}

void test_docker_exec() {
    STASIS_ASSERT(docker_exec("system info", 0) == 0, "unable to print docker information");
    STASIS_ASSERT(docker_exec("system info", STASIS_DOCKER_QUIET) == 0, "unable to be quiet");
}

void test_docker_sanitize_tag() {
    const char *data = "  !\"#$%&'()*+,-;<=>?@[\\]^_`{|}~";
    char *input = strdup(data);
    docker_sanitize_tag(input);
    int result = 0;
    for (size_t i = 0; i < strlen(data); i++) {
        if (input[i] != '-') {
            result = 1;
            break;
        }
    }
    STASIS_ASSERT(result == 0, "proper tag characters were not replaced correctly");
    guard_free(input);
}

void test_docker_build_and_script_and_save() {
    STASIS_SKIP_IF(docker_exec("pull alpine:latest", STASIS_DOCKER_QUIET), "unable to pull an image");

    const char *dockerfile_contents = "FROM alpine:latest\nCMD [\"sh\", \"-l\"]\n";
    mkdir("test_docker_build", 0755);
    if (!pushd("test_docker_build")) {
        stasis_testing_write_ascii("Dockerfile", dockerfile_contents);
        STASIS_ASSERT(docker_build(".", "-t test_docker_build", cap_suite.build) == 0, "docker build test failed");
        STASIS_ASSERT(docker_script("test_docker_build", "uname -a", 0) == 0, "simple docker container script execution failed");
        STASIS_ASSERT(docker_save("test_docker_build", ".", STASIS_DOCKER_IMAGE_COMPRESSION) == 0, "saving a simple image failed");
        STASIS_ASSERT(docker_exec("load < test_docker_build.tar.*", 0) == 0, "loading a simple image failed");
        docker_exec("image rm -f test_docker_build", 0);
        remove("Dockerfile");
        popd();
        remove("test_docker_build");
    } else {
        STASIS_ASSERT(false, "dir change failed");
        return;
    }
}

void test_docker_validate_compression_program() {
    STASIS_ASSERT(docker_validate_compression_program(STASIS_DOCKER_IMAGE_COMPRESSION) == 0, "baked-in compression program does not exist");
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    if (!docker_capable(&cap_suite)) {
        return STASIS_TEST_SUITE_SKIP;
    }
    STASIS_TEST_FUNC *tests[] = {
            test_docker_capable,
            test_docker_exec,
            test_docker_build_and_script_and_save,
            test_docker_sanitize_tag,
            test_docker_validate_compression_program,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}