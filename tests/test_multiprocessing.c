#include "testing.h"

static struct MultiProcessingPool *pool;
char *commands[] = {
        "true",
        "uname -a",
        "/bin/echo hello world",
};

void test_mp_pool_init() {
    STASIS_ASSERT((pool = mp_pool_init("mypool", "mplogs")) != NULL, "Pool initialization failed");
    STASIS_ASSERT_FATAL(pool != NULL, "Should not be NULL");
    STASIS_ASSERT(pool->num_alloc == MP_POOL_TASK_MAX, "Wrong number of default records");
    STASIS_ASSERT(pool->num_used == 0, "Wrong number of used records");
    STASIS_ASSERT(strcmp(pool->log_root, "mplogs") == 0, "Wrong log root directory");
    STASIS_ASSERT(strcmp(pool->ident, "mypool") == 0, "Wrong identity");

    int data_bad_total = 0;
    for (size_t i = 0; i < pool->num_alloc; i++) {
        int data_bad = 0;
        struct MultiProcessingTask *task = &pool->task[i];

        data_bad += task->status == 0 ? 0 : 1;
        data_bad += task->pid == 0 ? 0 : 1;
        data_bad += task->parent_pid == 0 ? 0 : 1;
        data_bad += task->signaled_by == 0 ? 0 : 1;
        data_bad += task->time_data.t_start.tv_nsec == 0 ? 0 : 1;
        data_bad += task->time_data.t_start.tv_sec == 0 ? 0 : 1;
        data_bad += task->time_data.t_stop.tv_nsec == 0 ? 0 : 1;
        data_bad += task->time_data.t_stop.tv_sec == 0 ? 0 : 1;
        data_bad += (int) strlen(task->ident) == 0 ? 0 : 1;
        data_bad += (int) strlen(task->parent_script) == 0 ? 0 : 1;
        data_bad += task->gate == NULL ? 0 : 1;
        if (data_bad) {
            SYSERROR("%s.task[%zu] has garbage values!", pool->ident, i);
            SYSERROR("  ident: %s", task->ident);
            SYSERROR("  status: %d", task->status);
            SYSERROR("  pid: %d", task->pid);
            SYSERROR("  parent_pid: %d", task->parent_pid);
            SYSERROR("  signaled_by: %d", task->signaled_by);
            SYSERROR("  t_start.tv_nsec: %ld", task->time_data.t_start.tv_nsec);
            SYSERROR("  t_start.tv_sec: %ld", task->time_data.t_start.tv_sec);
            SYSERROR("  t_stop.tv_nsec: %ld", task->time_data.t_stop.tv_nsec);
            SYSERROR("  t_stop.tv_sec: %ld", task->time_data.t_stop.tv_sec);
            SYSERROR("  gate: %s", task->gate == NULL ? "UNINITIALIZED (OK)" : "INITIALIZED (BAD)");
            data_bad_total++;
        }
    }
    STASIS_ASSERT(data_bad_total == 0, "Task array is not pristine");
    mp_pool_free(&pool);
}

void test_mp_task() {
    pool = mp_pool_init("mypool", "mplogs");

    if (pool) {
        for (size_t i = 0; i < sizeof(commands) / sizeof(*commands); i++) {
            struct MultiProcessingTask *task;
            char task_name[100] = {0};
            sprintf(task_name, "mytask%zu", i);
            STASIS_ASSERT_FATAL((task = mp_pool_task(pool, task_name, commands[i])) != NULL, "Task should not be NULL");
            STASIS_ASSERT(task->pid != 0, "PID should be non-zero at this point");
            STASIS_ASSERT(task->parent_pid != MP_POOL_PID_UNUSED, "Parent PID should be non-zero");
            STASIS_ASSERT(task->status == -1, "Status should be -1 (not started yet)");
            STASIS_ASSERT(strcmp(task->ident, task_name) == 0, "Wrong task identity");
            STASIS_ASSERT(strstr(task->log_file, pool->log_root) != NULL, "Log file path must be in log_root");
            STASIS_ASSERT(task->gate != NULL, "Semaphore should be initialized");
        }
    }
}

void test_mp_pool_join() {
    STASIS_ASSERT(mp_pool_join(pool, get_cpu_count(), 0) == 0, "Pool tasks should have not have failed");
    for (size_t i = 0; i < pool->num_used; i++) {
        struct MultiProcessingTask *task = &pool->task[i];
        STASIS_ASSERT(task->pid == MP_POOL_PID_UNUSED, "Task should be marked as unused");
        STASIS_ASSERT(task->status == 0, "Task status should be zero (success)");
    }
}

void test_mp_pool_free() {
    mp_pool_free(&pool);
    STASIS_ASSERT(pool == NULL, "Should be NULL");
}

void test_mp_pool_workflow() {
    struct testcase {
        const char *input_cmd;
        int input_join_flags;
        int expected_result;
        int expected_status;
        int expected_signal;
    };
    struct testcase tc[] = {
        {.input_cmd = "true && kill $$", .input_join_flags = 0, .expected_result = 1, .expected_status = 0, .expected_signal = SIGTERM},
        {.input_cmd = "false || kill $$", .input_join_flags = 0, .expected_result = 1, .expected_status = 0, .expected_signal = SIGTERM},
        {.input_cmd = "true", .input_join_flags = 0,.expected_result = 0, .expected_status = 0, .expected_signal = 0},
        {.input_cmd = "false", .input_join_flags = 0, .expected_result = 1, .expected_status = 1, .expected_signal = 0},
    };
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        struct testcase *test = &tc[i];
        struct MultiProcessingPool *p;
        struct MultiProcessingTask *task;
        STASIS_ASSERT((p = mp_pool_init("workflow", "mplogs")) != NULL, "Failed to initialize pool");
        STASIS_ASSERT((task = mp_pool_task(p, "task", (char *) test->input_cmd)) != NULL, "Failed to queue task");
        STASIS_ASSERT(mp_pool_join(p, get_cpu_count(), test->input_join_flags) == test->expected_result, "Unexpected result");
        STASIS_ASSERT(task->status == test->expected_status, "Unexpected status");
        STASIS_ASSERT(task->signaled_by == test->expected_signal, "Unexpected signal");
        STASIS_ASSERT(task->pid == MP_POOL_PID_UNUSED, "Unexpected PID. Should be marked UNUSED.");
        mp_pool_show_summary(p);
        mp_pool_free(&p);
    }
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_mp_pool_init,
        test_mp_task,
        test_mp_pool_join,
        test_mp_pool_free,
        test_mp_pool_workflow,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}