//
// Created by jhunk on 5/14/23.
//

#include "conda.h"

int micromamba(struct MicromambaInfo *info, char *command, ...) {
    struct utsname sys;
    uname(&sys);

    tolower_s(sys.sysname);
    if (!strcmp(sys.sysname, "darwin")) {
        strcpy(sys.sysname, "osx");
    }

    if (!strcmp(sys.machine, "x86_64")) {
        strcpy(sys.machine, "64");
    }

    char url[PATH_MAX];
    sprintf(url, "https://micro.mamba.pm/api/micromamba/%s-%s/latest", sys.sysname, sys.machine);

    char installer_path[PATH_MAX];
    sprintf(installer_path, "%s/latest", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");

    if (access(installer_path, F_OK)) {
        download(url, installer_path, NULL);
    }

    char mmbin[PATH_MAX];
    sprintf(mmbin, "%s/micromamba", info->micromamba_prefix);

    if (access(mmbin, F_OK)) {
        char untarcmd[PATH_MAX * 2];
        mkdirs(info->micromamba_prefix, 0755);
        sprintf(untarcmd, "tar -xvf %s -C %s --strip-components=1 bin/micromamba 1>/dev/null", installer_path, info->micromamba_prefix);
        int untarcmd_status = system(untarcmd);
        if (untarcmd_status) {
            return -1;
        }
    }

    char cmd[STASIS_BUFSIZ] = {0};
    sprintf(cmd, "%s -r %s -p %s ", mmbin, info->conda_prefix, info->conda_prefix);
    va_list args;
    va_start(args, command);
    vsprintf(cmd + strlen(cmd), command, args);
    va_end(args);

    mkdirs(info->conda_prefix, 0755);

    char rcpath[PATH_MAX];
    sprintf(rcpath, "%s/.condarc", info->conda_prefix);
    touch(rcpath);

    setenv("CONDARC", rcpath, 1);
    setenv("MAMBA_ROOT_PREFIX", info->conda_prefix, 1);
    int status = system(cmd);
    unsetenv("MAMBA_ROOT_PREFIX");

    return status;
}

int python_exec(const char *args) {
    char command[PATH_MAX] = {0};
    snprintf(command, sizeof(command) - 1, "python %s", args);
    msg(STASIS_MSG_L3, "Executing: %s\n", command);
    return system(command);
}

int pip_exec(const char *args) {
    char command[PATH_MAX] = {0};
    snprintf(command, sizeof(command) - 1, "python -m pip %s", args);
    msg(STASIS_MSG_L3, "Executing: %s\n", command);
    return system(command);
}

static const char *PKG_ERROR_STR[] = {
    "success",
    "[internal] unhandled package manager mode",
    "[internal] unable to create temporary log for process output",
    "package manager encountered an error at runtime",
    "package manager received a signal",
    "package manager failed to execute",
};

const char *pkg_index_provides_strerror(int code) {
    code = -code - (-PKG_INDEX_PROVIDES_ERROR_MESSAGE_OFFSET);
    return PKG_ERROR_STR[code];
}

int pkg_index_provides(int mode, const char *index, const char *spec) {
    char cmd[PATH_MAX] = {0};
    char spec_local[255] = {0};

    if (isempty((char *) spec)) {
        // NULL or zero-length; no package spec means there's nothing to do.
        return PKG_NOT_FOUND;
    }

    // Normalize the local spec string
    strncpy(spec_local, spec, sizeof(spec_local) - 1);
    tolower_s(spec_local);
    lstrip(spec_local);
    strip(spec_local);

    char logfile[] = "/tmp/STASIS-package_exists.XXXXXX";
    int logfd = mkstemp(logfile);
    if (logfd < 0) {
        perror(logfile);
        remove(logfile);  // fail harmlessly if not present
        return PKG_INDEX_PROVIDES_E_INTERNAL_LOG_HANDLE;
    }

    int status = 0;
    struct Process proc = {0};
    proc.redirect_stderr = 1;
    strcpy(proc.f_stdout, logfile);

    if (mode == PKG_USE_PIP) {
        // Do an installation in dry-run mode to see if the package exists in the given index.
        strncpy(cmd, "python -m pip install --dry-run --no-cache --no-deps ", sizeof(cmd) - 1);
        if (index) {
            snprintf(cmd + strlen(cmd), (sizeof(cmd) - 1) - strlen(cmd), "--index-url='%s' ", index);
        }
        snprintf(cmd + strlen(cmd), (sizeof(cmd) - 1) - strlen(cmd), "'%s' ", spec_local);
    } else if (mode == PKG_USE_CONDA) {
        strncpy(cmd, "mamba search ", sizeof(cmd) - 1);
        if (index) {
            snprintf(cmd + strlen(cmd), (sizeof(cmd) - 1) - strlen(cmd), "--channel '%s' ", index);
        }
        snprintf(cmd + strlen(cmd), (sizeof(cmd) - 1) - strlen(cmd), "'%s' ", spec_local);
    } else {
        return PKG_INDEX_PROVIDES_E_INTERNAL_MODE_UNKNOWN;
    }

    // Print errors only when shell() itself throws one
    // If some day we want to see the errors thrown by pip too, use this
    // condition instead:  (status != 0)
    status = shell(&proc, cmd);
    if (status < 0) {
        FILE *fp = fdopen(logfd, "r");
        if (!fp) {
            remove(logfile);
            return -1;
        } else {
            char line[BUFSIZ] = {0};
            fflush(stdout);
            fflush(stderr);
            while (fgets(line, sizeof(line) - 1, fp) != NULL) {
                fprintf(stderr, "%s", line);
            }
            fflush(stderr);
            fclose(fp);
        }
    }
    remove(logfile);

    if (WTERMSIG(proc.returncode)) {
        // This gets its own return value because if the external program
        // received a signal, even its status is zero, it's not reliable
        return PKG_INDEX_PROVIDES_E_MANAGER_SIGNALED;
    }

    if (status < 0) {
        return PKG_INDEX_PROVIDES_E_MANAGER_EXEC;
    } else if (WEXITSTATUS(proc.returncode) > 1) {
        // Pip and conda both return 2 on argument parsing errors
        return PKG_INDEX_PROVIDES_E_MANAGER_RUNTIME;
    } else if (WEXITSTATUS(proc.returncode) == 1) {
        // Pip and conda both return 1 when a package is not found.
        // Unfortunately this applies to botched version specs, too.
        return PKG_NOT_FOUND;
    } else {
        return PKG_FOUND;
    }
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
    char conda_as[6] = {0};

    strcpy(conda_as, "conda");
    for (size_t i = 0; mamba_commands[i] != NULL; i++) {
        if (startswith(args, mamba_commands[i])) {
            strcpy(conda_as, "mamba");
            break;
        }
    }

    snprintf(command, sizeof(command) - 1, "%s %s", conda_as, args);
    msg(STASIS_MSG_L3, "Executing: %s\n", command);
    return system(command);
}

static int conda_prepend_bin(const char *root) {
    char conda_bin[PATH_MAX] = {0};

    snprintf(conda_bin, sizeof(conda_bin) - 1, "%s/bin", root);
    if (env_manipulate_pathstr("PATH", conda_bin, PM_PREPEND | PM_ONCE)) {
        return -1;
    }
    return 0;
}

static int conda_prepend_condabin(const char *root) {
    char conda_condabin[PATH_MAX] = {0};

    snprintf(conda_condabin, sizeof(conda_condabin) - 1, "%s/condabin", root);
    if (env_manipulate_pathstr("PATH", conda_condabin, PM_PREPEND | PM_ONCE)) {
        return -1;
    }
    return 0;
}

static int env0_to_runtime(const char *logfile) {
    FILE *fp = fopen(logfile, "r");
    if (!fp) {
        perror(logfile);
        return -1;
    }

    while (!feof(fp)) {
        char buf[STASIS_BUFSIZ] = {0};
        int ch = 0;
        size_t z = 0;
        // We are ingesting output from "env -0" and can't use fgets()
        // Copy each character into the buffer until we encounter '\0' or EOF
        while (z < sizeof(buf) && (ch = (int) fgetc(fp)) != 0) {
            if (ch == EOF) {
                break;
            }
            buf[z] = (char) ch;
            z++;
        }
        buf[strlen(buf)] = 0;

        if (!strlen(buf)) {
            continue;
        }

        char **part = split(buf, "=", 1);
        if (!part) {
            perror("unable to split environment variable buffer");
            return -1;
        }
        if (!part[0]) {
            msg(STASIS_MSG_WARN | STASIS_MSG_L1, "Invalid environment variable key ignored: '%s'\n", buf);
        } else if (!part[1]) {
            msg(STASIS_MSG_WARN | STASIS_MSG_L1, "Invalid environment variable value ignored: '%s'\n", buf);
        } else {
            setenv(part[0], part[1], 1);
        }
        GENERIC_ARRAY_FREE(part);
    }
    fclose(fp);
    return 0;
}

int conda_activate(const char *root, const char *env_name) {
    const char *init_script_conda = "/etc/profile.d/conda.sh";
    const char *init_script_mamba = "/etc/profile.d/mamba.sh";
    char path_conda[PATH_MAX] = {0};
    char path_mamba[PATH_MAX] = {0};
    char logfile[PATH_MAX] = {0};
    struct Process proc = {0};

    // Where to find conda's init scripts
    sprintf(path_conda, "%s%s", root, init_script_conda);
    sprintf(path_mamba, "%s%s", root, init_script_mamba);

    // Set the path to our stdout log
    // Emulate mktemp()'s behavior. Give us a unique file name, but don't use
    // the file handle at all. We'll open it as a FILE stream soon enough.
    sprintf(logfile, "%s/%s", globals.tmpdir, "shell_XXXXXX");

    int fd = mkstemp(logfile);
    if (fd < 0) {
       perror(logfile);
       return -1;
    }
    close(fd);

    // Configure our process for output to a log file
    strcpy(proc.f_stdout, logfile);

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
    char command[PATH_MAX * 3];
    const char *conda_shlvl_str = getenv("CONDA_SHLVL");
    unsigned long conda_shlvl = 0;
    if (conda_shlvl_str) {
        conda_shlvl = strtol(conda_shlvl_str, NULL, 10);
    }

    if (conda_prepend_condabin(root)) {
        remove(logfile);
        return -1;
    }

    if (conda_prepend_bin(root)) {
        remove(logfile);
        return -1;
    }

    snprintf(command, sizeof(command) - 1,
        "set -a\n"
        "source %s\n"
        "__conda_exe() (\n"
        "    \"$CONDA_PYTHON_EXE\" \"$CONDA_EXE\" $_CE_M $_CE_CONDA \"$@\"\n"
        ")\n\n"
        "export -f __conda_exe\n"
        "source %s\n"
        "__mamba_exe() (\n"
        "    \\local MAMBA_CONDA_EXE_BACKUP=$CONDA_EXE\n"
        "    \\local MAMBA_EXE=$(\\dirname \"${CONDA_EXE}\")/mamba\n"
        "    \"$CONDA_PYTHON_EXE\" \"$MAMBA_EXE\" $_CE_M $_CE_CONDA \"$@\"\n"
        ")\n\n"
        "export -f __mamba_exe\n"
        "%s\n"
        "conda activate %s 1>&2\n"
        "env -0\n", path_conda, path_mamba, conda_shlvl ? "conda deactivate" : ":", env_name);

    int retval = shell(&proc, command);
    if (retval) {
        // it didn't work; drop out for cleanup
        remove(logfile);
        return retval;
    }

    // Parse the log file:
    // 1. Extract the environment keys and values from the sub-shell
    // 2. Apply it to STASIS's runtime environment
    if (env0_to_runtime(logfile) < 0) {
        return -1;
    }
    remove(logfile);
    return 0;
}

int conda_check_required() {
    int status = 0;
    struct StrList *result = NULL;
    char cmd[PATH_MAX] = {0};
    const char *conda_minimum_viable_tools[] = {
            "boa",
            "conda-build",
            NULL
    };

    // Construct a "conda list" command that searches for all required packages
    // using conda's (python's) regex matching
    strcat(cmd, "conda list '");
    for (size_t i = 0; conda_minimum_viable_tools[i] != NULL; i++) {
        strcat(cmd, "^");
        strcat(cmd, conda_minimum_viable_tools[i]);
        if (conda_minimum_viable_tools[i + 1] != NULL) {
            strcat(cmd, "|");
        }
    }
    strcat(cmd, "' | cut -d ' ' -f 1");

    // Verify all required packages are installed
    char *cmd_out = shell_output(cmd, &status);
    if (cmd_out) {
        size_t found = 0;
        result = strlist_init();
        strlist_append_tokenize(result, cmd_out, "\n");
        for (size_t i = 0; i < strlist_count(result); i++) {
            char *item = strlist_item(result, i);
            if (isempty(item) || startswith(item, "#")) {
                continue;
            }

            for (size_t x = 0; conda_minimum_viable_tools[x] != NULL; x++) {
                if (!strcmp(item, conda_minimum_viable_tools[x])) {
                    found++;
                }
            }
        }
        if (found < (sizeof(conda_minimum_viable_tools) / sizeof(*conda_minimum_viable_tools)) - 1) {
            guard_free(cmd_out);
            guard_strlist_free(&result);
            return 1;
        }
        guard_free(cmd_out);
        guard_strlist_free(&result);
    } else {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "The base package requirement check could not be performed\n");
        return 2;
    }
    return 0;
}

int conda_setup_headless() {
    if (globals.verbose) {
        conda_exec("config --system --set quiet false");
    } else {
        // Not verbose, so squelch conda's noise
        conda_exec("config --system --set quiet true");
    }

    // Configure conda for headless CI
    conda_exec("config --system --set auto_update_conda false");     // never update conda automatically
    conda_exec("config --system --set notify_outdated_conda false"); // never notify about outdated conda version
    conda_exec("config --system --set always_yes true");             // never prompt for input
    conda_exec("config --system --set safety_checks disabled");      // speedup
    conda_exec("config --system --set rollback_enabled false");      // speedup
    conda_exec("config --system --set report_errors false");         // disable data sharing
    conda_exec("config --system --set solver libmamba");             // use a real solver

    char cmd[PATH_MAX];
    size_t total = 0;
    if (globals.conda_packages && strlist_count(globals.conda_packages)) {
        memset(cmd, 0, sizeof(cmd));
        strcpy(cmd, "install ");

        total = strlist_count(globals.conda_packages);
        for (size_t i = 0; i < total; i++) {
            char *item = strlist_item(globals.conda_packages, i);
            if (isempty(item)) {
                continue;
            }
            sprintf(cmd + strlen(cmd), "'%s'", item);
            if (i < total - 1) {
                strcat(cmd, " ");
            }
        }

        if (conda_exec(cmd)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Unable to install user-defined base packages (conda)\n");
            return 1;
        }
    }

    if (globals.pip_packages && strlist_count(globals.pip_packages)) {
        memset(cmd, 0, sizeof(cmd));
        strcpy(cmd, "install ");

        total = strlist_count(globals.pip_packages);
        for (size_t i = 0; i < total; i++) {
            char *item = strlist_item(globals.pip_packages, i);
            if (isempty(item)) {
                continue;
            }
            sprintf(cmd + strlen(cmd), "'%s'", item);
            if (i < total - 1) {
                strcat(cmd, " ");
            }
        }

        if (pip_exec(cmd)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Unable to install user-defined base packages (pip)\n");
            return 1;
        }
    }

    if (conda_check_required()) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Your STASIS configuration lacks the bare"
                                                  " minimum software required to build conda packages."
                                                  " Please fix it.\n");
        return 1;
    }

    if (globals.always_update_base_environment) {
        if (conda_exec("update --all")) {
            fprintf(stderr, "conda update was unsuccessful\n");
            return 1;
        }
    }

    return 0;
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

char *conda_get_active_environment() {
    const char *name = getenv("CONDA_DEFAULT_ENV");
    if (!name) {
        return NULL;
    }

    char *result = NULL;
    result = strdup(name);
    if (!result) {
        return NULL;
    }

    return result;
}

int conda_index(const char *path) {
    char command[PATH_MAX];
    sprintf(command, "index %s", path);
    return conda_exec(command);
}
