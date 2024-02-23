#ifndef OMC_DOCKER_H
#define OMC_DOCKER_H

#define OMC_DOCKER_QUIET 1 << 1

#define OMC_DOCKER_BUILD 1 << 1
#define OMC_DOCKER_BUILD_X 1 << 2

struct DockerCapabilities {
    int podman;
    int build;
    int available;
    int usable;
};

int docker_capable(struct DockerCapabilities *result);
int docker_exec(const char *args, unsigned flags);
int docker_build(const char *dirpath, const char *args, int engine);
int docker_script(const char *image, char *data, unsigned flags);
int docker_save(const char *image, const char *destdir);

#endif //OMC_DOCKER_H
