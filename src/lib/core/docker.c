#include "docker.h"


int docker_exec(const char *args, unsigned flags) {
    struct Process proc;
    char cmd[PATH_MAX];

    memset(&proc, 0, sizeof(proc));
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd) - 1, "docker %s", args);
    if (flags & STASIS_DOCKER_QUIET) {
        strcpy(proc.f_stdout, "/dev/null");
        strcpy(proc.f_stderr, "/dev/null");
    } else {
        msg(STASIS_MSG_L2, "Executing: %s\n", cmd);
    }

    shell(&proc, cmd);
    return proc.returncode;
}

int docker_script(const char *image, char *data, unsigned flags) {
    (void)flags;  // TODO: placeholder
    char cmd[PATH_MAX] = {0};

    snprintf(cmd, sizeof(cmd) - 1, "docker run --rm -i %s /bin/sh -", image);

    FILE *outfile = popen(cmd, "w");
    if (!outfile) {
        // opening command pipe for writing failed
        return -1;
    }

    FILE *infile = fmemopen(data, strlen(data), "r");
    if (!infile) {
        // opening memory file for reading failed
        return -1;
    }

    do {
        char buffer[STASIS_BUFSIZ] = {0};
        if (fgets(buffer, sizeof(buffer) - 1, infile) != NULL) {
            fputs(buffer, outfile);
        }
    } while (!feof(infile));

    fclose(infile);
    return pclose(outfile);
}

int docker_build(const char *dirpath, const char *args, int engine) {
    char cmd[PATH_MAX];
    char build[15] = {0};

    memset(cmd, 0, sizeof(cmd));

    if (engine & STASIS_DOCKER_BUILD) {
        strcpy(build, "build");
    }
    if (engine & STASIS_DOCKER_BUILD_X) {
        strcpy(build, "buildx build");
    }
    snprintf(cmd, sizeof(cmd) - 1, "%s %s %s", build, args, dirpath);
    return docker_exec(cmd, 0);
}

int docker_save(const char *image, const char *destdir, const char *compression_program) {
    char cmd[PATH_MAX] = {0};

    if (compression_program && strlen(compression_program)) {
        char ext[255] = {0};
        if (startswith(compression_program, "zstd")) {
            strcpy(ext, "zst");
        } else if (startswith(compression_program, "xz")) {
            strcpy(ext, "xz");
        } else if (startswith(compression_program, "gzip")) {
            strcpy(ext, "gz");
        } else if (startswith(compression_program, "bzip2")) {
            strcpy(ext, "bz2");
        } else {
            strncpy(ext, compression_program, sizeof(ext) - 1);
        }
        sprintf(cmd, "save \"%s\" | %s > \"%s/%s.tar.%s\"", image, compression_program, destdir, image, ext);
    } else {
        sprintf(cmd, "save \"%s\" -o \"%s/%s.tar\"", image, destdir, image);

    }
    return docker_exec(cmd, 0);
}

static int docker_exists() {
    if (find_program("docker")) {
        return true;
    }
    return false;
}

static char *docker_ident() {
    FILE *fp = NULL;
    char *tempfile = NULL;
    char line[PATH_MAX];
    struct Process proc;

    tempfile = xmkstemp(&fp, "w+");
    if (!fp || !tempfile) {
        return NULL;
    }

    memset(&proc, 0, sizeof(proc));
    strcpy(proc.f_stdout, tempfile);
    strcpy(proc.f_stderr, "/dev/null");
    shell(&proc, "docker --version");

    if (!freopen(tempfile, "r", fp)) {
        remove(tempfile);
        guard_free(tempfile);
        return NULL;
    }

    if (!fgets(line, sizeof(line) - 1, fp)) {
        fclose(fp);
        remove(tempfile);
        guard_free(tempfile);
        return NULL;
    }

    fclose(fp);
    remove(tempfile);
    guard_free(tempfile);

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

    if (docker_exec("ps", STASIS_DOCKER_QUIET)) {
        // user cannot connect to the socket
        return false;
    }

    version = docker_ident();
    if (version && startswith(version, "podman")) {
        result->podman = true;
    }
    guard_free(version);

    if (!docker_exec("buildx build --help", STASIS_DOCKER_QUIET)) {
        result->build |= STASIS_DOCKER_BUILD_X;
    }
    if (!docker_exec("build --help", STASIS_DOCKER_QUIET)) {
        result->build |= STASIS_DOCKER_BUILD;
    }
    if (!result->build) {
        // can't use docker without a build plugin
        return false;
    }
    result->usable = true;
    return true;
}

void docker_sanitize_tag(char *str) {
    char *pos = str;
    while (*pos != 0) {
        if (!isalnum(*pos)) {
            if (*pos != '.' && *pos != ':' && *pos != '/') {
                *pos = '-';
            }
        }
        pos++;
    }
}

int docker_validate_compression_program(char *prog) {
    int result = -1;
    char **parts = NULL;
    if (!prog) {
        goto invalid;
    }
    parts = split(prog, " ", 1);
    if (!parts) {
        goto invalid;
    }
    result = find_program(parts[0]) ? 0 : -1;

    invalid:
    GENERIC_ARRAY_FREE(parts);
    return result;
}
