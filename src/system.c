//
// Created by jhunk on 10/4/23.
//

#include "system.h"

int shell(struct Process *proc, char *args[]) {
    FILE *fp_out, *fp_err;
    pid_t pid;
    pid_t status;
    status = 0;
    errno = 0;

    pid = fork();
    if (pid == -1) {
        fprintf(stderr, "fork failed\n");
        exit(1);
    } else if (pid == 0) {
        int retval;
        if (proc != NULL) {
            if (strlen(proc->stdout)) {
                fp_out = freopen(proc->stdout, "w+", stdout);
            }

            if (strlen(proc->stderr)) {
                fp_err = freopen(proc->stderr, "w+", stderr);
            }

            if (proc->redirect_stderr) {
                if (fp_err) {
                    fclose(fp_err);
                    fclose(stderr);
                }
                dup2(fileno(stdout), fileno(stderr));
            }
        }

        retval = execv(args[0], args);
        fprintf(stderr, "# executing: ");
        for (size_t x = 0; args[x] != NULL; x++) {
            fprintf(stderr, "%s ", args[x]);
        }

        if (proc != NULL && strlen(proc->stdout)) {
            fflush(fp_out);
            fclose(fp_out);
            fflush(stdout);
            fclose(stdout);
        }
        if (proc != NULL && strlen(proc->stderr)) {
            fflush(fp_err);
            fclose(fp_err);
            fflush(stderr);
            fclose(stderr);
        }
        exit(retval);
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


    if (proc != NULL) {
        proc->returncode = status;
    }
    return WEXITSTATUS(status);
}

int shell2(struct Process *proc, char *args) {
    FILE *fp_out = NULL;
    FILE *fp_err = NULL;
    pid_t pid;
    pid_t status;
    status = 0;
    errno = 0;

    char t_name[PATH_MAX];
    strcpy(t_name, "/tmp/ohmycal.XXXXXX");
    int fd = mkstemp(t_name);

    FILE *tp;
    tp = fdopen(fd, "w");
    if (!tp) {
        return -1;
    }

    fprintf(tp, "#!/bin/bash\n%s\n", args);
    fflush(tp);
    fclose(tp);
    close(fd);
    chmod(t_name, 0755);

    pid = fork();
    if (pid == -1) {
        fprintf(stderr, "fork failed\n");
        exit(1);
    } else if (pid == 0) {
        int retval;
        if (proc != NULL) {
            if (strlen(proc->stdout)) {
                fp_out = freopen(proc->stdout, "w+", stdout);
            }

            if (strlen(proc->stderr)) {
                fp_err = freopen(proc->stderr, "w+", stderr);
            }

            if (proc->redirect_stderr) {
                if (fp_err) {
                    fclose(fp_err);
                    fclose(stderr);
                }
                dup2(fileno(stdout), fileno(stderr));
            }
        }

        retval = execl("/bin/bash", "bash", "-c", t_name, (char *) NULL);
        if (!access(t_name, F_OK)) {
            remove(t_name);
        }

        if (proc != NULL && strlen(proc->stdout)) {
            if (fp_out != NULL) {
                fflush(fp_out);
                fclose(fp_out);
            }
            fflush(stdout);
            fclose(stdout);
        }
        if (proc != NULL && strlen(proc->stderr)) {
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

    if (proc != NULL) {
        proc->returncode = status;
    }
    return WEXITSTATUS(status);
}

int shell_safe(struct Process *proc, char *args[]) {
    FILE *fp;
    char buf[1024] = {0};
    int result;

    for (size_t i = 0; args[i] != NULL; i++) {
        if (strpbrk(args[i], ";&|()")) {
            args[i] = NULL;
            break;
        }
    }

    result = shell(proc, args);
    if (strlen(proc->stdout)) {
        fp = fopen(proc->stdout, "r");
        if (fp) {
            while (fgets(buf, sizeof(buf) - 1, fp)) {
                fprintf(stdout, "%s", buf);
                buf[0] = '\0';
            }
            fclose(fp);
            fp = NULL;
        }
    }
    if (strlen(proc->stderr)) {
        fp = fopen(proc->stderr, "r");
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
