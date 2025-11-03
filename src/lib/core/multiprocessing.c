#include "core.h"
#include "multiprocessing.h"

/// The sum of all tasks started by mp_task()
size_t mp_global_task_count = 0;

static struct MultiProcessingTask *mp_pool_next_available(struct MultiProcessingPool *pool) {
    return &pool->task[pool->num_used];
}

int child(struct MultiProcessingPool *pool, struct MultiProcessingTask *task) {
    FILE *fp_log = NULL;

    // The task starts inside the requested working directory
    if (chdir(task->working_dir)) {
        perror(task->working_dir);
        exit(1);
    }

    // Record the task start time
    if (clock_gettime(CLOCK_REALTIME, &task->time_data.t_start) < 0) {
        perror("clock_gettime");
        exit(1);
    }

    // Redirect stdout and stderr to the log file
    fflush(stdout);
    fflush(stderr);
    // Set log file name
    sprintf(task->log_file + strlen(task->log_file), "task-%zu-%d.log", mp_global_task_count, task->parent_pid);
    fp_log = freopen(task->log_file, "w+", stdout);
    if (!fp_log) {
        fprintf(stderr, "unable to open '%s' for writing: %s\n", task->log_file, strerror(errno));
        return -1;
    }
    dup2(fileno(stdout), fileno(stderr));

    // Generate timestamp for log header
    time_t t = time(NULL);
    char *timebuf = ctime(&t);
    if (timebuf) {
        // strip line feed from timestamp
        timebuf[strlen(timebuf) ? strlen(timebuf) - 1 : 0] = 0;
    }

    // Generate log header
    fprintf(fp_log, "# STARTED: %s\n", timebuf ? timebuf : "unknown");
    fprintf(fp_log, "# PID: %d\n", task->parent_pid);
    fprintf(fp_log, "# WORKDIR: %s\n", task->working_dir);
    fprintf(fp_log, "# COMMAND:\n%s\n", task->cmd);
    fprintf(fp_log, "# OUTPUT:\n");
    // Commit header to log file / clean up
    fflush(fp_log);

    // Execute task
    fflush(stdout);
    fflush(stderr);
    char *args[] = {"bash", "--norc", task->parent_script, (char *) NULL};
    return execvp("/bin/bash", args);
}

int parent(struct MultiProcessingPool *pool, struct MultiProcessingTask *task, pid_t pid, int *child_status) {
    printf("[%s:%s] Task started (pid: %d)\n", pool->ident, task->ident, pid);

    // Give the child process access to our PID value
    task->pid = pid;
    task->parent_pid = pid;

    mp_global_task_count++;

    // Check child's status
    pid_t code = waitpid(pid, child_status, WUNTRACED | WCONTINUED | WNOHANG);
    if (code < 0) {
        perror("waitpid failed");
        return -1;
    }
    return 0;
}

static int mp_task_fork(struct MultiProcessingPool *pool, struct MultiProcessingTask *task) {
    SYSDEBUG("Preparing to fork() child task %s:%s", pool->ident, task->ident);
    semaphore_wait(&pool->semaphore);
    pid_t pid = fork();
    int parent_status = 0;
    int child_status = 0;
    if (pid == -1) {
        return -1;
    }
    if (pid == 0) {
        child(pool, task);
    } else {
        parent_status = parent(pool, task, pid, &child_status);
        fflush(stdout);
        fflush(stderr);
    }
    semaphore_post(&pool->semaphore);
    return parent_status;
}

struct MultiProcessingTask *mp_pool_task(struct MultiProcessingPool *pool, const char *ident, char *working_dir, char *cmd) {
    SYSDEBUG("%s", "Finding next available slot");
    struct MultiProcessingTask *slot = mp_pool_next_available(pool);
    if (pool->num_used != pool->num_alloc) {
        SYSDEBUG("Using slot %zu of %zu", pool->num_used, pool->num_alloc);
        pool->num_used++;
    } else {
        fprintf(stderr, "Maximum number of tasks reached\n");
        return NULL;
    }

    // Set default status to "error"
    slot->status = MP_POOL_TASK_STATUS_INITIAL;

    // Set task identifier string
    memset(slot->ident, 0, sizeof(slot->ident));
    strncpy(slot->ident, ident, sizeof(slot->ident) - 1);

    // Set log file path
    memset(slot->log_file, 0, sizeof(*slot->log_file));
    strcat(slot->log_file, pool->log_root);
    strcat(slot->log_file, "/");

    // Set working directory
    if (isempty(working_dir)) {
        strcpy(slot->working_dir, ".");
    } else {
        strncpy(slot->working_dir, working_dir, PATH_MAX - 1);
    }

    // Create a temporary file to act as our intermediate command script
    FILE *tp = NULL;
    char *t_name = NULL;

    t_name = xmkstemp(&tp, "w");
    if (!t_name || !tp) {
        return NULL;
    }

    // Set the script's permissions so that only the calling user can use it
    // This should help prevent eavesdropping if keys are applied in plain-text
    // somewhere.
    chmod(t_name, 0700);

    // Record the script path
    memset(slot->parent_script, 0, sizeof(slot->parent_script));
    strncpy(slot->parent_script, t_name, PATH_MAX - 1);
    guard_free(t_name);

    // Populate the script
    SYSDEBUG("Generating runner script: %s", slot->parent_script);
    fprintf(tp, "#!/bin/bash\n%s\n", cmd);
    fflush(tp);
    fclose(tp);

    // Record the command(s)
    SYSDEBUG("%s", "mmap() slot command");
    slot->cmd_len = (strlen(cmd) * sizeof(*cmd)) + 1;
    slot->cmd = mmap(NULL, slot->cmd_len, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(slot->cmd, 0, slot->cmd_len);
    strncpy(slot->cmd, cmd, slot->cmd_len);

    return slot;
}

static void get_task_duration(struct MultiProcessingTask *task, struct timespec *result) {
    // based on the timersub() macro in time.h
    // This implementation uses timespec and increases the resolution from microseconds to nanoseconds.
    struct timespec *start = &task->time_data.t_start;
    struct timespec *stop = &task->time_data.t_stop;
    result->tv_sec = (stop->tv_sec - start->tv_sec);
    result->tv_nsec = (stop->tv_nsec - start->tv_nsec);
    if (result->tv_nsec < 0) {
        --result->tv_sec;
        result->tv_nsec += 1000000000L;
    }
}

void mp_pool_show_summary(struct MultiProcessingPool *pool) {
    print_banner("=", 79);
    printf("Pool execution summary for \"%s\"\n", pool->ident);
    print_banner("=", 79);
    printf("STATUS     PID     DURATION     IDENT\n");
    for (size_t i = 0; i < pool->num_used; i++) {
        struct MultiProcessingTask *task = &pool->task[i];
        char status_str[10] = {0};

        if (task->status == MP_POOL_TASK_STATUS_INITIAL && task->pid == MP_POOL_PID_UNUSED) {
            // You will only see this label if the task pool is killed by
            // MP_POOL_FAIL_FAST and tasks are still queued for execution
            strcpy(status_str, "HOLD");
        } else if (!task->status && !task->signaled_by) {

            strcpy(status_str, "DONE");
        } else if (task->signaled_by) {
            strcpy(status_str, "TERM");
        } else {
            strcpy(status_str, "FAIL");
        }

        struct timespec duration;
        get_task_duration(task, &duration);
        long diff = duration.tv_sec + duration.tv_nsec / 1000000000L;
        printf("%-4s   %10d  %7lds     %-10s\n", status_str, task->parent_pid, diff, task->ident) ;
    }
    puts("");
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
    fflush(stream);
    fclose(fp);
    return 0;
}

int mp_pool_kill(struct MultiProcessingPool *pool, int signum) {
    printf("Sending signal %d to pool '%s'\n", signum, pool->ident);
    for (size_t i = 0; i < pool->num_used; i++) {
        struct MultiProcessingTask *slot = &pool->task[i];
        if (!slot) {
            return -1;
        }
        // Kill tasks in progress
        if (slot->pid > 0) {
            int status;
            printf("Sending signal %d to task '%s' (pid: %d)\n", signum, slot->ident, slot->pid);
            status = kill(slot->pid, signum);
            if (status && errno != ESRCH) {
                fprintf(stderr, "Task '%s' (pid: %d) did not respond: %s\n", slot->ident, slot->pid, strerror(errno));
            } else {
                // Wait for process to handle the signal, then set the status accordingly
                if (waitpid(slot->pid, &status, 0) >= 0) {
                    slot->signaled_by = WTERMSIG(status);
                    // Record the task stop time
                    if (clock_gettime(CLOCK_REALTIME, &slot->time_data.t_stop) < 0) {
                        perror("clock_gettime");
                        exit(1);
                    }
                    // We are short-circuiting the normal flow, and the process is now dead, so mark it as such
                    SYSDEBUG("Marking slot %zu: UNUSED", i);
                    slot->pid = MP_POOL_PID_UNUSED;
                }
            }
        }
        if (!access(slot->log_file, F_OK)) {
            SYSDEBUG("Removing log file: %s", slot->log_file);
            remove(slot->log_file);
        }
        if (!access(slot->parent_script, F_OK)) {
            SYSDEBUG("Removing runner script: %s", slot->parent_script);
            remove(slot->parent_script);
        }
    }
    return 0;
}

int mp_pool_join(struct MultiProcessingPool *pool, size_t jobs, size_t flags) {
    int status = 0;
    int failures = 0;
    size_t tasks_complete = 0;
    size_t lower_i = 0;
    size_t upper_i = jobs;

    do {
        size_t hang_check = 0;
        if (upper_i >= pool->num_used) {
            size_t x = upper_i - pool->num_used;
            upper_i -= (size_t) x;
        }

        for (size_t i = lower_i; i < upper_i; i++) {
            struct MultiProcessingTask *slot = &pool->task[i];
            if (slot->status == MP_POOL_TASK_STATUS_INITIAL) {
                slot->_startup = time(NULL);
                if (mp_task_fork(pool, slot)) {
                    fprintf(stderr, "%s: mp_task_fork failed\n", slot->ident);
                    kill(0, SIGTERM);
                }
            }

            // Has the child been processed already?
            if (slot->pid == MP_POOL_PID_UNUSED) {
                // Child is already used up, skip it
                hang_check++;
                SYSDEBUG("slot %zu: hang_check=%zu", i, hang_check);
                if (hang_check >= pool->num_used) {
                    // If you join a pool that's already finished it will spin
                    // forever. This protects the program from entering an
                    // infinite loop.
                    fprintf(stderr, "%s is deadlocked\n", pool->ident);
                    failures++;
                    goto pool_deadlocked;
                }
                continue;
            }

            // Is the process finished?
            pid_t pid = waitpid(slot->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
            int task_ended = WIFEXITED(status);
            int task_ended_by_signal = WIFSIGNALED(status);
            int task_stopped = WIFSTOPPED(status);
            int task_continued = WIFCONTINUED(status);
            int status_exit = WEXITSTATUS(status);
            int status_signal = WTERMSIG(status);
            int status_stopped = WSTOPSIG(status);

            // Update status
            slot->status = status_exit;
            slot->signaled_by = status_signal;

            char progress[1024] = {0};
            if (pid > 0) {
                double percent = ((double) (tasks_complete + 1) / (double) pool->num_used) * 100;
                snprintf(progress, sizeof(progress) - 1, "[%s:%s] [%3.1f%%]", pool->ident, slot->ident, percent);

                // The process ended in one the following ways
                // Note: SIGSTOP nor SIGCONT will not increment the tasks_complete counter
                if (task_stopped) {
                    printf("%s Task was suspended (%d)\n", progress, status_stopped);
                    continue;
                } else if (task_continued) {
                    printf("%s Task was resumed\n", progress);
                    continue;
                } else if (task_ended_by_signal) {
                    printf("%s Task ended by signal %d (%s)\n", progress, status_signal, strsignal(status_signal));
                    tasks_complete++;
                } else if (task_ended) {
                    printf("%s Task ended (status: %d)\n", progress, status_exit);
                    tasks_complete++;
                } else {
                    fprintf(stderr, "%s Task state is unknown (0x%04X)\n", progress, status);
                }

                // Show the log (always)
                if (show_log_contents(stdout, slot)) {
                    perror(slot->log_file);
                }

                // Record the task stop time
                if (clock_gettime(CLOCK_REALTIME, &slot->time_data.t_stop) < 0) {
                    perror("clock_gettime");
                    exit(1);
                }

                if (status >> 8 != 0 || (status & 0xff) != 0) {
                    fprintf(stderr, "%s Task failed after %lus\n", progress, slot->elapsed);
                    failures++;

                    if (flags & MP_POOL_FAIL_FAST && pool->num_used > 1) {
                        mp_pool_kill(pool, SIGTERM);
                        return -2;
                    }
                } else {
                    printf("%s Task finished after %lus\n", progress, slot->elapsed);
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
            } else if (pid < 0) {
                fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
                return -1;
            } else {
                // Track the number of seconds elapsed for each task.
                // When a task has executed for longer than status_intervals, print a status update
                // _seconds represents the time between intervals, not the total runtime of the task
                slot->_seconds = time(NULL) - slot->_now;
                if (slot->_seconds >  pool->status_interval) {
                    slot->_now = time(NULL);
                    slot->_seconds = 0;
                }
                if (slot->_seconds == 0) {
                    printf("[%s:%s] Task is running (pid: %d, elapsed: %lus)\n", pool->ident, slot->ident, slot->parent_pid, slot->elapsed);
                }
            }
            slot->elapsed = time(NULL) - slot->_startup;
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

    pool_deadlocked:
    puts("");

    return failures;
}


struct MultiProcessingPool *mp_pool_init(const char *ident, const char *log_root) {
    struct MultiProcessingPool *pool;

    if (!ident || !log_root) {
        // Pool must have an ident string
        // log_root must be set
        return NULL;
    }

    // The pool is shared with children
    pool = mmap(NULL, sizeof(*pool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    // Set pool identity string
    memset(pool->ident, 0, sizeof(pool->ident));
    strncpy(pool->ident, ident, sizeof(pool->ident) - 1);

    // Set logging base directory
    memset(pool->log_root, 0, sizeof(pool->log_root));
    strncpy(pool->log_root, log_root, sizeof(pool->log_root) - 1);
    pool->num_used = 0;
    pool->num_alloc = MP_POOL_TASK_MAX;

    // Create the log directory
    if (mkdirs(log_root, 0700) < 0) {
        if (errno != EEXIST) {
            perror(log_root);
            mp_pool_free(&pool);
            return NULL;
        }
    }

    // Task array is shared with children
    pool->task = mmap(NULL, (pool->num_alloc + 1) * sizeof(*pool->task), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (pool->task == MAP_FAILED) {
        perror("mmap");
        mp_pool_free(&pool);
        return NULL;
    }

    char semaphore_name[255] = {0};
    snprintf(semaphore_name, sizeof(semaphore_name), "stasis_mp_%s", ident);
    if (semaphore_init(&pool->semaphore, semaphore_name, 2) != 0) {
        fprintf(stderr, "unable to initialize semaphore\n");
        mp_pool_free(&pool);
        return NULL;
    }

    return pool;
}

void mp_pool_free(struct MultiProcessingPool **pool) {
    for (size_t i = 0; i < (*pool)->num_alloc; i++) {
    }
    semaphore_destroy(&(*pool)->semaphore);

    // Unmap all pool tasks
    if ((*pool)->task) {
        if ((*pool)->task->cmd) {
            if (munmap((*pool)->task->cmd, (*pool)->task->cmd_len) < 0) {
                perror("munmap");
            }
        }
        if (munmap((*pool)->task, sizeof(*(*pool)->task) * (*pool)->num_alloc) < 0) {
            perror("munmap");
        }
    }
    // Unmap the pool
    if ((*pool)) {
        if (munmap((*pool), sizeof(*(*pool))) < 0) {
            perror("munmap");
        }
        (*pool) = NULL;
    }
}