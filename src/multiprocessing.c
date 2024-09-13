#include "core.h"

size_t mp_global_task_count = 0;
static struct MultiProcessingTask *mp_pool_next_available(struct MultiProcessingPool *pool) {
    return &pool->task[pool->num_used];
}

struct MultiProcessingTask *mp_task(struct MultiProcessingPool *pool, const char *ident, char *cmd) {
    struct MultiProcessingTask *slot = mp_pool_next_available(pool);
    //struct MultiProcessingTask *slot = mp_pool_any_available(pool);
    pool->num_used++;

    memset(slot->ident, 0, sizeof(slot->ident));
    strncpy(slot->ident, ident, sizeof(slot->ident) - 1);

    strcat(slot->log_file, pool->log_root);
    strcat(slot->log_file, "/");

    FILE *tp = NULL;
    char *t_name;
    t_name = xmkstemp(&tp, "w");
    if (!t_name || !tp) {
        return NULL;
    }

    // Set the script's permissions so that only the calling user can use it
    // This should help prevent eavesdropping if keys are applied in plain-text
    // somewhere.
    chmod(t_name, 0700);

    memset(slot->parent_script, 0, sizeof(slot->parent_script));
    strncpy(slot->parent_script, t_name, PATH_MAX - 1);

    fprintf(tp, "#!/bin/bash\n%s\n", cmd);
    fflush(tp);
    fclose(tp);

    char sema_name[PATH_MAX] = {0};
    sprintf(sema_name, "/sem-%zu-%s-%s", mp_global_task_count, pool->ident, slot->ident);
    sem_unlink(sema_name);
    slot->gate = sem_open(sema_name, O_CREAT, 0644, 0);
    if (slot->gate == SEM_FAILED) {
        perror("sem_open failed");
        exit(1);
    }

    pid_t pid = fork();
    int child_status = 0;
    if (pid == -1) {
        return NULL;
    } else if (pid == 0) { // child process
        char *cwd = NULL;
        FILE *fp_log = NULL;

        // Synchronize sub-process startup
        // Stop here until summoned by mp_pool_join()
        if (sem_wait(slot->gate) < 0) {
            perror("sem_wait failed");
            exit(1);
        }


        // Redirect stdout and stderr to the log file
        fflush(stdout);
        fflush(stderr);
        printf("[%s:%s] Task started (pid: %d)\n", pool->ident, slot->ident, slot->parent_pid);
        fp_log = freopen(slot->log_file, "w+", stdout);
        if (!fp_log) {
            fprintf(stderr, "unable to open '%s' for writing: %s\n", slot->log_file, strerror(errno));
            return NULL;
        }
        dup2(fileno(stdout), fileno(stderr));

        // Generate timestamp for log header
        time_t t = time(NULL);
        char *timebuf = ctime(&t);
        if (timebuf) {
            // strip line feed from timestamp
            timebuf[strlen(timebuf) ? strlen(timebuf) - 1 : 0] = 0;
        }

        cwd = getcwd(NULL, PATH_MAX);
        if (!cwd) {
            perror(cwd);
            exit(1);
        }

        // Generate log header
        fprintf(fp_log, "# STARTED: %s\n", timebuf ? timebuf : "unknown");
        fprintf(fp_log, "# PID: %d\n", slot->parent_pid);
        fprintf(fp_log, "# WORKDIR: %s\n", cwd);
        fprintf(fp_log, "# COMMAND:\n%s\n", cmd);
        fprintf(fp_log, "# OUTPUT:\n");
        // Commit header to log file / clean up
        fflush(fp_log);
        guard_free(cwd);

        // Execute task
        fflush(stdout);
        fflush(stderr);
        char *args[] = {"bash", "--norc", t_name, (char *) NULL};
        slot->status = execvp("/bin/bash", args);
    } else { // parent process
        printf("[%s:%s] Task queued (pid: %d)\n", pool->ident, slot->ident, pid);

        // Give the child process access to our PID value
        slot->pid = pid;
        slot->parent_pid = pid;

        // Set log file name
        sprintf(slot->log_file + strlen(slot->log_file), "task-%zu-%d.log", mp_global_task_count, slot->parent_pid);
        mp_global_task_count++;

        // Check child's status
        pid_t code = waitpid(pid, &child_status, WUNTRACED | WCONTINUED | WNOHANG);
        if (code < 0) {
            perror("waitpid failed");
            return NULL;
        }
        return slot;
    }
    return NULL;
}

static int show_log_contents(FILE *stream, struct MultiProcessingTask *task) {
    FILE *fp = fopen(task->log_file, "r");
    if (!fp) {
        return -1;
    }
    char buf[BUFSIZ] = {0};
    while ((fgets(buf, sizeof(buf) - 1, fp)) != NULL) {
        fprintf(stream, "%s", buf);
        memset(buf, 0, sizeof(buf));
    }
    fprintf(stream, "\n");
    return 0;
}

int mp_pool_kill(struct MultiProcessingPool *pool, int signum) {
    printf("Sending signal %d to pool '%s'\n", signum, pool->ident);
    //for (size_t i = 0; i < pool->num_alloc; i++) {
    for (size_t i = 0; i < pool->num_used; i++) {
        struct MultiProcessingTask *slot = &pool->task[i];
        if (!slot) {
            return -1;
        }
        if (slot->pid > 0) {
            int status;
            printf("Sending signal %d to task '%s' (pid: %d)\n", signum, slot->ident, slot->pid);
            status = kill(slot->pid, signum);
            if (status && errno != ESRCH) {
                fprintf(stderr, "Task '%s' (pid: %d) did not respond: %s\n", slot->ident, slot->pid, strerror(errno));
            }
        }
        if (!access(slot->log_file, F_OK)) {
            remove(slot->log_file);
        }
        if (!access(slot->parent_script, F_OK)) {
            remove(slot->parent_script);
        }
    }
    return 0;
}

int mp_pool_join(struct MultiProcessingPool *pool, size_t jobs, size_t flags) {
    int status = 0;
    int watcher = 0;
    int failures = 0;
    size_t tasks_complete = 0;
    size_t lower_i = 0;
    size_t upper_i = jobs;

    do {
        //if (upper_i >= pool->num_alloc) {
        if (upper_i >= pool->num_used) {
            size_t x = upper_i - pool->num_used;
            //size_t x = upper_i - pool->num_alloc;
            upper_i -= (size_t) x;
        }
        for (size_t i = lower_i; i < upper_i; i++) {
            struct MultiProcessingTask *slot = &pool->task[i];
            // Unlock the semaphore to allow the queued processes to start up
            if (sem_post(slot->gate) < 0) {
                perror("sem_post failed");
                exit(1);
            }

            if (slot->pid == MP_POOL_PID_UNUSED) {
                // PID is already used up, skip it
                continue;
            }

            // Is the process finished?
            pid_t pid = waitpid(slot->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

            // Update status
            slot->status = status;

            char progress[1024] = {0};
            if (pid > 0) {
                double percent = ((double) (tasks_complete) / (double) pool->num_used) * 100;
                //double percent = ((double) (tasks_complete) / (double) pool->num_alloc) * 100;
                sprintf(progress, "[%s:%s] [%3.1f%%]", pool->ident, slot->ident, percent);

                // The process ended in one the following ways
                // Note: SIGSTOP nor SIGCONT will not increment the tasks_complete counter
                if (WIFEXITED(status)) {
                    printf("%s Task finished (status: %d)\n", progress, WEXITSTATUS(status));
                    tasks_complete++;
                } else if (WIFSIGNALED(status)) {
                    printf("%s Task ended by signal %d (%s)\n", progress, WTERMSIG(status), strsignal(WTERMSIG(status)));
                    tasks_complete++;
                } else if (WIFSTOPPED(status)) {
                    printf("%s Task was suspended (%d)\n", progress, WSTOPSIG(status));
                    continue;
                } else if (WIFCONTINUED(status)) {
                    printf("%s Task was resumed\n", progress);
                    continue;
                } else {
                    fprintf(stderr, "%s Task state is unknown (0x%04X)\n", progress, status);
                }

                // Show the log (always)
                if (show_log_contents(stdout, slot)) {
                    perror(slot->log_file);
                }
                if (status >> 8 != 0 || (status & 0xff) != 0) {
                    fprintf(stderr, "%s Task failed\n", progress);
                    if (flags & MP_POOL_FAIL_FAST) {
                        mp_pool_kill(pool, SIGTERM);
                        return -2;
                    }
                } else {
                    printf("%s Task finished\n", progress);
                }

                // Clean up logs and scripts left behind by the task
                if (remove(slot->log_file)) {
                    fprintf(stderr, "%s Unable to remove log file: '%s': %s\n", progress, slot->parent_script, strerror(errno));
                }
                if (remove(slot->parent_script)) {
                    fprintf(stderr, "%s Unable to remove temporary script '%s': %s\n", progress, slot->parent_script, strerror(errno));
                }

                // Update progress and tell the poller to ignore the PID. The process is gone.
                slot->pid = MP_POOL_PID_UNUSED;
                failures += status;
            } else if (pid < 0) {
                fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
                return -1;
            } else {
                if (watcher > 9) {
                    printf("[%s:%s] Task is running (pid: %d)\n", pool->ident, slot->ident, slot->parent_pid);
                    watcher = 0;
                } else {
                    watcher++;
                }
            }
        }

        if (tasks_complete == pool->num_used) {
            break;
        }

        if (tasks_complete == upper_i) {
            lower_i += jobs;
            upper_i += jobs;
        }

        // Poll again after a short delay
        sleep(1);
    } while (1);

    puts("");
    return failures;
}


struct MultiProcessingPool *mp_pool_init(const char *ident, const char *log_root) {
    struct MultiProcessingPool *pool;

    if (!log_root) {
        // log_root must be set
        return NULL;
    }

    pool = mmap(NULL, sizeof(*pool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(pool->ident, 0, sizeof(pool->ident));
    strncpy(pool->ident, ident, sizeof(pool->ident) - 1);

    memset(pool->log_root, 0, sizeof(pool->log_root));
    strncpy(pool->log_root, log_root, sizeof(pool->log_root) - 1);
    pool->num_used = 0;
    pool->num_alloc = MP_POOL_TASK_MAX;

    if (mkdirs(log_root, 0700) < 0) {
        if (errno != EEXIST) {
            perror(log_root);
            mp_pool_free(&pool);
            return NULL;
        }
    }

    pool->task = mmap(NULL, (pool->num_alloc + 1) * sizeof(*pool->task), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (pool->task == MAP_FAILED) {
        perror("mmap");
        mp_pool_free(&pool);
        return NULL;
    }

    return pool;
}

void mp_pool_free(struct MultiProcessingPool **pool) {
    for (size_t i = 0; i < (*pool)->num_alloc; i++) {
        if ((*pool)->task[i].gate) {
            if (sem_close((*pool)->task[i].gate) < 0) {
                perror("sem_close failed");
                exit(1);
            }
        }
    }
    if ((*pool)->task) {
        if (munmap((*pool)->task, sizeof(*(*pool)->task) * (*pool)->num_alloc) < 0) {
            perror("munmap");
        }
    }
    if ((*pool)) {
        if (munmap((*pool), sizeof(*(*pool))) < 0) {
            perror("munmap");
        }
        (*pool) = NULL;
    }
}

int exmain(int argc, char *argv[]) {
    size_t i;
    struct MultiProcessingPool *pool = mp_pool_init("generic", "logs");
    if (!pool) {
        perror("pool init");
        exit(1);
    }

    char *commands[] = {
        "sleep 2; dd if=/dev/zero of=file.dat bs=1M count=1",
        "/bin/echo hello world; sleep 5",
        "python -c 'print(1+1)'",
        "(for x in {1..10}; do echo $x; sleep 0.5; done)",
        "echo stdout >&1; echo stderr >&2; exit 1"
    };

    for (i = 0; i < sizeof(commands) / sizeof(*commands); i++) {
        if (mp_task(pool, "commands array", commands[i]) == NULL) {
            printf("Too many tasks queued (max: %d)\n", MP_POOL_TASK_MAX);
            break;
        }
    }
    mp_pool_join(pool, get_cpu_count() - 1, MP_POOL_FAIL_FAST);
    mp_pool_free(&pool);
    return 0;
}
