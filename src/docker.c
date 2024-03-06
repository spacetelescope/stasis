#include "omc.h"
#include "docker.h"


int docker_exec(const char *args, unsigned flags) {
    struct Process proc;
    char cmd[PATH_MAX];

    memset(&proc, 0, sizeof(proc));
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd) - 1, "docker %s", args);
    if (flags & OMC_DOCKER_QUIET) {
        strcpy(proc.f_stdout, "/dev/null");
        strcpy(proc.f_stderr, "/dev/null");
    } else {
        msg(OMC_MSG_L2, "Executing: %s\n", cmd);
    }

    shell(&proc, cmd);
    return proc.returncode;
}

int docker_script(const char *image, char *data, unsigned flags) {
    FILE *infile;
    FILE *outfile;
    char cmd[PATH_MAX];
    char buffer[OMC_BUFSIZ];

    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd) - 1, "docker run --rm -i %s /bin/bash -", image);

    outfile = popen(cmd, "w");
    if (!outfile) {
        // opening command pipe for writing failed
        return -1;
    }

    infile = fmemopen(data, strlen(data), "r");
    if (!infile) {
        // opening memory file for reading failed
        return -1;
    }

    do {
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, sizeof(buffer) - 1, infile);
        fputs(buffer, outfile);
    } while (!feof(infile));

    fclose(infile);
    return pclose(outfile);
}

int docker_build(const char *dirpath, const char *args, int engine) {
    char cmd[PATH_MAX];
    char build[15];

    memset(build, 0, sizeof(build));
    memset(cmd, 0, sizeof(cmd));

    if (engine & OMC_DOCKER_BUILD) {
        strcpy(build, "build");
    }
    if (engine & OMC_DOCKER_BUILD_X) {
        strcpy(build, "buildx build");
    }
    snprintf(cmd, sizeof(cmd) - 1, "%s %s %s", build, args, dirpath);
    return docker_exec(cmd, 0);
}

int docker_save(const char *image, const char *destdir) {
    char cmd[PATH_MAX];
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "save %s -o \"%s/%s.tar\"", image, destdir, image);
    return docker_exec(cmd, 0);
}

static int docker_exists() {
    if (find_program("docker")) {
        return true;
    }
    return false;
}

char *docker_ident() {
    FILE *fp = NULL;
    char *tempfile = NULL;
    char line[PATH_MAX];
    struct Process proc;

    tempfile = xmkstemp(&fp, "w");
    if (!fp || !tempfile) {
        return NULL;
    }

    memset(&proc, 0, sizeof(proc));
    strcpy(proc.f_stdout, tempfile);
    strcpy(proc.f_stderr, "/dev/null");
    shell(&proc, "docker -v");

    if (!freopen(tempfile, "r", fp)) {
        return NULL;
    }

    if (!fgets(line, sizeof(line) - 1, fp)) {
        free(tempfile);
        return NULL;
    }
    fclose(fp);
    remove(tempfile);

    return strdup(line);
}

int docker_capable(struct DockerCapabilities *result) {
    char *version = NULL;
    memset(result, 0, sizeof(*result));

    if (!docker_exists()) {
        // docker isn't available
        return false;
    }
    result->available = true;

    if (docker_exec("ps", OMC_DOCKER_QUIET)) {
        // user cannot connect to the socket
        return false;
    }

    version = docker_ident();
    if (version && startswith(version, "podman")) {
        result->podman = true;
    }
    guard_free(version)

    if (!docker_exec("buildx build --help", OMC_DOCKER_QUIET)) {
        result->build |= OMC_DOCKER_BUILD;
    }
    if (!docker_exec("build --help", OMC_DOCKER_QUIET)) {
        result->build |= OMC_DOCKER_BUILD_X;
    }
    if (!result->build) {
        // can't use docker without a build plugin
        return false;
    }
    result->usable = true;
    return true;
}
