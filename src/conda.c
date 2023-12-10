//
// Created by jhunk on 5/14/23.
//

#include <unistd.h>
#include "conda.h"

extern struct OMC_GLOBAL globals;

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
    sprintf(logfile, "%s/%s", globals.tmpdir, "shell_XXXXXX");
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
        remove(logfile);
        return -1;
    }

    if (access(path_mamba, F_OK) < 0) {
        perror(path_mamba);
        remove(logfile);
        return -1;
    }

    // Fully activate conda and record its effect on the runtime environment
    char command[PATH_MAX];
    snprintf(command, sizeof(command) - 1, "source %s; source %s; conda activate %s &>/dev/null; env -0", path_conda, path_mamba, env_name);
    int retval = shell2(&proc, command);
    if (retval) {
        // it didn't work; drop out for cleanup
        remove(logfile);
        return retval;
    }

    // Parse the log file:
    // 1. Extract the environment keys and values from the sub-shell
    // 2. Apply it to OMC's runtime environment
    // 3. Now we're ready to execute conda commands anywhere
    fp = fopen(proc.stdout, "r");
    if (!fp) {
        perror(logfile);
        return -1;
    }
    int i = 0;
    while (!feof(fp)) {
        char buf[OMC_BUFSIZ] = {0};
        int ch = 0;
        size_t z = 0;
        while (z < sizeof(buf) && (ch = (int) fgetc(fp)) != 0) {
            if (ch == EOF) {
                ch = 0;
                break;
            }
            buf[z] = (char) ch;
            z++;
        }
        buf[strlen(buf)] = 0;

        if (!strlen(buf)) {
            continue;
        }
        //printf("\e[0m[%d] %s\e[0m\n", i, buf);
        char **part = split(buf, "=", 1);
        if (!part) {
            perror("unable to split environment variable buffer");
            return -1;
        }
        if (!part[0]) {
            msg(OMC_MSG_WARN | OMC_MSG_L1, "Invalid environment variable key ignored: '%s'\n", buf);
            i++;
        } else if (!part[1]) {
            msg(OMC_MSG_WARN | OMC_MSG_L1, "Invalid environment variable value ignored: '%s'\n", buf);
            i++;
        } else {
            setenv(part[0], part[1], 1);
        }
        split_free(part);
        i++;
    }
    fclose(fp);
    remove(logfile);
    return 0;
}

void conda_setup_headless() {
    if (globals.verbose) {
        conda_exec("config --system --set quiet false");
    } else {
        // Not verbose, so squelch conda's noise
        conda_exec("config --system --set quiet true");
    }

    // Configure conda for headless CI
    conda_exec("config --system --set auto_update_conda false");
    conda_exec("config --system --set always_yes true");
    conda_exec("config --system --set rollback_enabled false");
    conda_exec("config --system --set report_errors false");
    conda_exec("config --system --set solver libmamba");

    if (globals.verbose) {
        char *rcfile = getenv("CONDARC");
        if (rcfile) {
            msg(OMC_MSG_L1, "Dump %s\n", rcfile);
            char **condarc_contents = file_readlines(rcfile, 0, 0, NULL);
            for (size_t i = 0; condarc_contents[i] != NULL; i++) {
                msg(OMC_MSG_L2, "%s", condarc_contents[i]);
                free(condarc_contents[i]);
            }
            free(condarc_contents);
        }

    }

    if (globals.always_update_base_environment) {
        if (conda_exec("update --all")) {
            fprintf(stderr, "conda update was unsuccessful\n");
            exit(1);
        }
    }
}

int conda_env_create_from_uri(char *name, char *uri) {
    char env_command[PATH_MAX];
    sprintf(env_command, "env create -n %s -f %s", name, uri);
    return conda_exec(env_command);
}

int conda_env_create(char *name, char *python_version, char *packages) {
    char env_command[PATH_MAX];
    sprintf(env_command, "create -n %s python=%s %s", name, python_version, packages ? packages : "");
    return conda_exec(env_command);
}

int conda_env_remove(char *name) {
    char env_command[PATH_MAX];
    sprintf(env_command, "env remove -n %s", name);
    return conda_exec(env_command);
}

int conda_env_export(char *name, char *output_dir, char *output_filename) {
    char env_command[PATH_MAX];
    sprintf(env_command, "env export -n %s -f %s/%s.yml", name, output_dir, output_filename);
    return conda_exec(env_command);
}

int conda_index(const char *path) {
    char command[PATH_MAX];
    sprintf(command, "index %s", path);
    return conda_exec(command);
}
