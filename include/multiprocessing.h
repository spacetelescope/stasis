/// @file multiprocessing.h
#ifndef STASIS_MULTIPROCESSING_H
#define STASIS_MULTIPROCESSING_H

#include "core.h"
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

struct MultiProcessingTask {
    sem_t *gate; ///< Child process startup lock
    pid_t pid; ///< Program PID
    pid_t parent_pid; ///< Program PID (parent process)
    int status; ///< Child process exit status
    char ident[255]; ///< Identity of the pool task
    char log_file[255]; ///< Path to stdout/stderr log file
    char parent_script[PATH_MAX]; ///< Path to temporary script executing the task
};

struct MultiProcessingPool {
    struct MultiProcessingTask *task; ///< Array of tasks to execute
    size_t num_used; ///< Number of tasks populated in the task array
    size_t num_alloc; ///< Number of tasks allocated by the task array
    const char *ident; ///< Identity of task pool
    const char *log_root; ///< Base directory to store stderr/stdout log files
};

///!< Maximum number of multiprocessing tasks STASIS can execute
#define MP_POOL_TASK_MAX 1000

///!< Value signifies a process is unused or finished executing
#define MP_POOL_PID_UNUSED 0

// Option flags for mp_pool_join()
#define MP_POOL_FAIL_FAST 1 << 1

/**
 * Create a multiprocessing pool
 *
 * ```c
 * #include "multiprocessing.h"
 * #include "utils.h" // for get_cpu_count()
 *
 * int main(int argc, char *argv[]) {
 *     struct MultiProcessingPool *mp;
 *     mp = mp_pool_init("mypool", "/tmp/mypool_logs");
 *     if (mp) {
 *         char *commands[] = {
 *             "/bin/echo hello world",
 *             "/bin/echo world hello",
 *             NULL
 *         }
 *         for (size_t i = 0; commands[i] != NULL); i++) {
 *             struct MultiProcessingTask *task;
 *             char task_name[100];
 *
 *             sprintf(task_name, "mytask%zu", i);
 *             task = mp_task(mp, task_name, commands[i]);
 *             if (!task) {
 *                 // handle task creation error
 *             }
 *         }
 *         if (mp_pool_join(mp, get_cpu_count())) {
 *             // handle pool execution error
 *         }
 *         mp_pool_free(&mp);
 *     } else {
 *         // handle pool initialization error
 *     }
 * }
 * ```
 *
 * @param ident a name to identify the pool
 * @param log_root the path to store program output
 * @return pointer to initialized MultiProcessingPool
 * @return NULL on error
 */
struct MultiProcessingPool *mp_pool_init(const char *ident, const char *log_root);

/**
 * Create a multiprocessing pool task
 *
 * @param pool a pointer to MultiProcessingPool
 * @param ident a name to identify the task
 * @param cmd a command to execute
 * @return pointer to MultiProcessingTask structure
 * @return NULL on error
 */
struct MultiProcessingTask *mp_task(struct MultiProcessingPool *pool, const char *ident, char *cmd);

/**
 * Execute all tasks in a pool
 *
 * @param pool a pointer to MultiProcessingPool
 * @param jobs the number of processes to spawn at once (for serial execution use `1`)
 * @param flags option to be OR'd (MP_POOL_FAIL_FAST)
 * @return 0 on success
 * @return >0 on failure
 * @return <0 on error
 */
int mp_pool_join(struct MultiProcessingPool *pool, size_t jobs, size_t flags);

/**
 * Release resources allocated by mp_pool_init()
 *
 * @param a pointer to MultiProcessingPool
 */
void mp_pool_free(struct MultiProcessingPool **pool);


#endif //STASIS_MULTIPROCESSING_H