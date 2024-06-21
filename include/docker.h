//! @file docker.h
#ifndef STASIS_DOCKER_H
#define STASIS_DOCKER_H

//! Flag to squelch output from docker_exec()
#define STASIS_DOCKER_QUIET 1 << 1

//! Flag for older style docker build
#define STASIS_DOCKER_BUILD 1 << 1
//! Flag for docker buildx
#define STASIS_DOCKER_BUILD_X 1 << 2

//! Compress "docker save"ed images with a compression program
#define STASIS_DOCKER_IMAGE_COMPRESSION "zstd"

struct DockerCapabilities {
    int podman;  //!< Is "docker" really podman?
    int build;  //!< Is a build plugin available?
    int available;  //!< Is a "docker" program available?
    int usable;  //!< Is docker in a usable state for the current user?
};

/**
 * Determine the state of docker on the system
 *
 * ```c
 * struct DockerCapabilities docker_is;
 * if (!docker_capable(&docker_is)) {
 *     fprintf(stderr, "%s is %savailable, and %susable\n",
 *         docker_is.podman ? "Podman" : "Docker",
 *         docker_is.available ? "" : "not ",
 *         docker_is.usable ? "" : "not ");
 *     exit(1);
 * }
 * ```
 *
 * @param result DockerCapabilities struct
 * @return 1 on success, 0 on error
 */
int docker_capable(struct DockerCapabilities *result);

/**
 * Execute a docker command
 *
 * Use the `STASIS_DOCKER_QUIET` flag to suppress all output from stdout and stderr.
 *
 * ```c
 * if (docker_exec("run --rm -t ubuntu:latest /bin/bash -c 'echo Hello world'", 0)) {
 *     fprintf(stderr, "Docker hello world failed\n");
 *     exit(1);
 * }
 * ```
 *
 * @param args arguments to pass to docker
 * @param flags
 * @return exit code from "docker"
 */
int docker_exec(const char *args, unsigned flags);

/**
 * Build a docker image
 *
 * ```c
 * struct DockerCapabilities docker_is;
 * docker_capable(&docker_is);
 *
 * if (docker_is.usable) {
 *     printf("Building docker image\n");
 *     if (docker_build("path/to/Dockerfile/dir")) {
 *         fprintf("Docker build failed\n");
 *         exit(1);
 *     }
 * } else {
 *     fprintf(stderr, "No usable docker installation available\n");
 * }
 * ```
 *
 * @param dirpath
 * @param args
 * @param engine
 * @return
 */
int docker_build(const char *dirpath, const char *args, int engine);
int docker_script(const char *image, char *data, unsigned flags);
int docker_save(const char *image, const char *destdir, const char *compression_program);
void docker_sanitize_tag(char *str);
int docker_validate_compression_program(char *prog);


#endif //STASIS_DOCKER_H
