//
// Created by jhunk on 5/14/23.
//

#include <unistd.h>
#include "conda.h"

int python_exec(const char *args) {
    char command[PATH_MAX];
    memset(command, 0, sizeof(command));
    snprintf(command, sizeof(command) - 1, "python %s", args);
    msg(OMC_MSG_L3, "Executing: %s\n", command);
    return system(command);
}

int pip_exec(const char *args) {
    char command[PATH_MAX];
    memset(command, 0, sizeof(command));
    snprintf(command, sizeof(command) - 1, "python -m pip %s", args);
    msg(OMC_MSG_L3, "Executing: %s\n", command);
    return system(command);
}

int conda_exec(const char *args) {
    char command[PATH_MAX];
    const char *mamba_commands[] = {
            "build",
            "install",
            "update",
            "create",
            "list",
            "search",
            "run",
            "info",
            "clean",
            "activate",
            "deactivate",
            NULL
    };
    char conda_as[6];
    memset(conda_as, 0, sizeof(conda_as));

    strcpy(conda_as, "conda");
    for (size_t i = 0; mamba_commands[i] != NULL; i++) {
        if (startswith(args, mamba_commands[i])) {
            strcpy(conda_as, "mamba");
            break;
        }
    }

    snprintf(command, sizeof(command) - 1, "%s %s", conda_as, args);
    msg(OMC_MSG_L3, "Executing: %s\n", command);
    return system(command);
}

int conda_activate(const char *root, const char *env_name) {
    int fd = -1;
    FILE *fp = NULL;
    const char *init_script_conda = "/etc/profile.d/conda.sh";
    const char *init_script_mamba = "/etc/profile.d/mamba.sh";
    char path_conda[PATH_MAX] = {0};
    char path_mamba[PATH_MAX] = {0};
    char logfile[PATH_MAX] = {0};
    struct Process proc;
    memset(&proc, 0, sizeof(proc));

    // Where to find conda's init scripts
    sprintf(path_conda, "%s%s", root, init_script_conda);
    sprintf(path_mamba, "%s%s", root, init_script_mamba);

    // Set the path to our stdout log
    // Emulate mktemp()'s behavior. Give us a unique file name, but don't use
    // the file handle at all. We'll open it as a FILE stream soon enough.
    strcpy(logfile, "/tmp/shell_XXXXXX");
    fd = mkstemp(logfile);
    if (fd < 0) {
       perror(logfile);
       return -1;
    }
    close(fd);

    // Configure our process for output to a log file
    strcpy(proc.stdout, logfile);

    // Verify conda's init scripts are available
    if (access(path_conda, F_OK) < 0) {
        perror(path_conda);
        return -1;
    }

    if (access(path_mamba, F_OK) < 0) {
        perror(path_mamba);
        return -1;
    }

    // Fully activate conda and record its effect on the runtime environment
    char command[PATH_MAX];
    snprintf(command, sizeof(command) - 1, "source %s; source %s; conda activate %s &>/dev/null; printenv", path_conda, path_mamba, env_name);
    int retval = shell2(&proc, command);
    if (retval) {
        // it didn't work; drop out for cleanup
        return retval;
    }

    // Parse the log file:
    // 1. Extract the environment keys and values from the sub-shell
    // 2. Apply it to ohmycal's runtime environment
    // 3. Now we're ready to execute conda commands anywhere
    fp = fopen(proc.stdout, "r");
    if (!fp) {
        perror(logfile);
        return -1;
    }
    static char buf[1024];
    int i = 0;
    while (fgets(buf, sizeof(buf) -1, fp) != NULL) {
        buf[strlen(buf) - 1] = 0;
        if (!strlen(buf)) {
            continue;
        }
        //printf("[%d] %s\n", i, buf);
        char *eq = strchr(buf, '=');
        if (eq) {
            *eq = '\0';
        }
        char *key = buf;
        char *val = &eq[1];
        setenv(key, val, 1);
        i++;
    }
    fclose(fp);
    remove(logfile);
    return 0;
}

void conda_env_create_from_uri(char *name, char *uri) {
    char env_command[PATH_MAX];
    sprintf(env_command, "env create -n %s -f %s", name, uri);
    if (conda_exec(env_command)) {
        fprintf(stderr, "derived environment creation failed\n");
        exit(1);
    }
}

void conda_env_create(char *name, char *python_version, char *packages) {
    char env_command[PATH_MAX];
    sprintf(env_command, "create -n %s python=%s %s", name, python_version, packages ? packages : "");
    if (conda_exec(env_command)) {
        fprintf(stderr, "conda environment creation failed\n");
        exit(1);
    }
}

void conda_env_remove(char *name) {
    char env_command[PATH_MAX];
    sprintf(env_command, "env remove -n %s", name);
    if (conda_exec(env_command)) {
        fprintf(stderr, "conda environment removal failed\n");
        exit(1);
    }
}

void conda_env_export(char *name, char *output_dir, char *output_filename) {
    char env_command[PATH_MAX];
    sprintf(env_command, "env export -n %s -f %s/%s.yml", name, output_dir, output_filename);
    if (conda_exec(env_command)) {
        fprintf(stderr, "conda environment export failed\n");
        exit(1);
    }
}

int conda_index(const char *path) {
    char command[PATH_MAX];
    sprintf(command, "index %s", path);
    return conda_exec(command);
}
