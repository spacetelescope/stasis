#include "testing.h"
struct DockerCapabilities cap_suite;

void test_docker_capable() {
    struct DockerCapabilities cap;
    int result = docker_capable(&cap);
    OMC_ASSERT(result == true, "docker installation unusable");
    OMC_ASSERT(cap.available, "docker is not available");
    OMC_ASSERT(cap.build != 0, "docker cannot build images");
    // no cap.podman assertion. doesn't matter if we have docker or podman
    OMC_ASSERT(cap.usable, "docker is unusable");
}

void test_docker_exec() {
    OMC_ASSERT(docker_exec("system info", 0) == 0, "unable to print docker information");
    OMC_ASSERT(docker_exec("system info", OMC_DOCKER_QUIET) == 0, "unable to be quiet");
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
    OMC_ASSERT(result == 0, "proper tag characters were not replaced correctly");
    guard_free(input);
}

void test_docker_build_and_script_and_save() {
    const char *dockerfile_contents = "FROM alpine:latest\nCMD [\"sh\", \"-l\"]\n";
    mkdir("test_docker_build", 0755);
    if (!pushd("test_docker_build")) {
        omc_testing_write_ascii("Dockerfile", dockerfile_contents);
        OMC_ASSERT(docker_build(".", "--arch $(uname -m) -t test_docker_build", cap_suite.build) == 0, "docker build test failed");
        OMC_ASSERT(docker_script("test_docker_build", "uname -a", 0) == 0, "simple docker container script execution failed");
        OMC_ASSERT(docker_save("test_docker_build", ".", OMC_DOCKER_IMAGE_COMPRESSION) == 0, "saving a simple image failed");
        OMC_ASSERT(docker_exec("load < test_docker_build.tar.*", 0) == 0, "loading a simple image failed");
        docker_exec("image rm -f test_docker_build", 0);
        remove("Dockerfile");
        popd();
        remove("test_docker_build");
    } else {
        OMC_ASSERT(false, "dir change failed");
        return;
    }
}

void test_docker_validate_compression_program() {
    OMC_ASSERT(docker_validate_compression_program(OMC_DOCKER_IMAGE_COMPRESSION) == 0, "baked-in compression program does not exist");
}

int main(int argc, char *argv[]) {
    OMC_TEST_BEGIN_MAIN();
    if (!docker_capable(&cap_suite)) {
        return OMC_TEST_SUITE_SKIP;
    }
    OMC_TEST_FUNC *tests[] = {
            test_docker_capable,
            test_docker_exec,
            test_docker_build_and_script_and_save,
            test_docker_sanitize_tag,
            test_docker_validate_compression_program,
    };
    OMC_TEST_RUN(tests);
    OMC_TEST_END_MAIN();
}