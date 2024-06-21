#include "system.h"
#include "core.h"

int shell(struct Process *proc, char *args) {
    struct Process selfproc;
    FILE *fp_out = NULL;
    FILE *fp_err = NULL;
    pid_t pid;
    pid_t status;
    status = 0;
    errno = 0;

    if (!proc) {
        // provide our own proc structure
        // albeit not accessible to the user
        memset(&selfproc, 0, sizeof(selfproc));
        proc = &selfproc;
    }

    if (!args) {
        proc->returncode = -1;
        return -1;
    }

    FILE *tp = NULL;
    char *t_name;
    t_name = xmkstemp(&tp, "w");
    if (!t_name || !tp) {
        return -1;
    }

    fprintf(tp, "#!/bin/bash\n%s\n", args);
    fflush(tp);
    fclose(tp);
    chmod(t_name, 0755);

    pid = fork();
    if (pid == -1) {
        fprintf(stderr, "fork failed\n");
        exit(1);
    } else if (pid == 0) {
        int retval;
        if (strlen(proc->f_stdout)) {
            fp_out = freopen(proc->f_stdout, "w+", stdout);
        }

        if (strlen(proc->f_stderr)) {
            fp_err = freopen(proc->f_stderr, "w+", stderr);
        }

        if (proc->redirect_stderr) {
            if (fp_err) {
                fclose(fp_err);
                fclose(stderr);
            }
            dup2(fileno(stdout), fileno(stderr));
        }

        retval = execl("/bin/bash", "bash", "-c", t_name, (char *) NULL);
        if (!access(t_name, F_OK)) {
            remove(t_name);
        }

        if (strlen(proc->f_stdout)) {
            if (fp_out != NULL) {
                fflush(fp_out);
                fclose(fp_out);
            }
            fflush(stdout);
            fclose(stdout);
        }
        if (strlen(proc->f_stderr)) {
            if (fp_err) {
                fflush(fp_err);
                fclose(fp_err);
            }
            fflush(stderr);
            fclose(stderr);
        }
        return retval;
    } else {
        if (waitpid(pid, &status, WUNTRACED) > 0) {
            if (WIFEXITED(status) && WEXITSTATUS(status)) {
                if (WEXITSTATUS(status) == 127) {
                    fprintf(stderr, "execv failed\n");
                }
            } else if (WIFSIGNALED(status))  {
                fprintf(stderr, "signal received: %d\n", WIFSIGNALED(status));
            }
        } else {
            fprintf(stderr, "waitpid() failed\n");
        }
    }

    if (!access(t_name, F_OK)) {
        remove(t_name);
    }

    proc->returncode = status;
    guard_free(t_name);
    return WEXITSTATUS(status);
}

int shell_safe(struct Process *proc, char *args) {
    FILE *fp;
    char buf[1024] = {0};
    int result;

    char *invalid_ch = strpbrk(args, STASIS_SHELL_SAFE_RESTRICT);
    if (invalid_ch) {
        args = NULL;
    }

    result = shell(proc, args);
    if (strlen(proc->f_stdout)) {
        fp = fopen(proc->f_stdout, "r");
        if (fp) {
            while (fgets(buf, sizeof(buf) - 1, fp)) {
                fprintf(stdout, "%s", buf);
                buf[0] = '\0';
            }
            fclose(fp);
            fp = NULL;
        }
    }
    if (strlen(proc->f_stderr)) {
        fp = fopen(proc->f_stderr, "r");
        if (fp) {
            while (fgets(buf, sizeof(buf) - 1, fp)) {
                fprintf(stderr, "%s", buf);
                buf[0] = '\0';
            }
            fclose(fp);
            fp = NULL;
        }
    }
    return result;
}

char *shell_output(const char *command, int *status) {
    const size_t initial_size = STASIS_BUFSIZ;
    size_t current_size = initial_size;
    char *result = NULL;
    char line[STASIS_BUFSIZ];
    FILE *pp;

    errno = 0;
    *status = 0;
    pp = popen(command, "r");
    if (!pp) {
        *status = -1;
        return NULL;
    }

    if (errno) {
        *status = 1;
    }
    result = calloc(initial_size, sizeof(result));
    memset(line, 0, sizeof(line));
    while (fread(line, sizeof(char), sizeof(line) - 1, pp) != 0) {
        size_t result_len = strlen(result);
        size_t need_realloc = (result_len + strlen(line)) > current_size;
        if (need_realloc) {
            current_size += initial_size;
            char *tmp = realloc(result, sizeof(*result) * current_size);
            if (!tmp) {
                return NULL;
            } else if (tmp != result) {
                result = tmp;
            }
        }
        strcat(result, line);
        memset(line, 0, sizeof(line));
    }
    *status = pclose(pp);
    return result;
}