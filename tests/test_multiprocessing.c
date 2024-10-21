#include "testing.h"
#include "multiprocessing.h"
#include <pthread.h>

static struct MultiProcessingPool *pool;
char *commands[] = {
        "sleep 1; true",
        "sleep 2; uname -a",
        "sleep 3; /bin/echo hello world",
        "sleep 4; true",
        "sleep 5; uname -a",
        "sleep 6; /bin/echo hello world",
};

void test_mp_pool_init() {
    STASIS_ASSERT((pool = mp_pool_init(NULL, "mplogs")) == NULL, "Pool should not be initialized with invalid ident");
    STASIS_ASSERT((pool = mp_pool_init("mypool", NULL)) == NULL, "Pool should not be initialized with invalid logname");
    STASIS_ASSERT((pool = mp_pool_init(NULL, NULL)) == NULL, "Pool should not be initialized with invalid arguments");
    pool = NULL;
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
            data_bad_total++;
        }
    }
    STASIS_ASSERT(data_bad_total == 0, "Task array is not pristine");
    mp_pool_free(&pool);
}

void test_mp_task() {
    pool = mp_pool_init("mypool", "mplogs");

    if (pool) {
        pool->status_interval = 3;
        for (size_t i = 0; i < sizeof(commands) / sizeof(*commands); i++) {
            struct MultiProcessingTask *task;
            char task_name[100] = {0};
            sprintf(task_name, "mytask%zu", i);
            STASIS_ASSERT_FATAL((task = mp_pool_task(pool, task_name, NULL, commands[i])) != NULL, "Task should not be NULL");
            STASIS_ASSERT(task->pid == MP_POOL_PID_UNUSED, "PID should be non-zero at this point");
            STASIS_ASSERT(task->parent_pid == MP_POOL_PID_UNUSED, "Parent PID should be non-zero");
            STASIS_ASSERT(task->status == -1, "Status should be -1 (not started yet)");
            STASIS_ASSERT(strcmp(task->ident, task_name) == 0, "Wrong task identity");
            STASIS_ASSERT(strstr(task->log_file, pool->log_root) != NULL, "Log file path must be in log_root");
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
        STASIS_ASSERT((task = mp_pool_task(p, "task", NULL, (char *) test->input_cmd)) != NULL, "Failed to queue task");
        STASIS_ASSERT(mp_pool_join(p, get_cpu_count(), test->input_join_flags) == test->expected_result, "Unexpected result");
        STASIS_ASSERT(task->status == test->expected_status, "Unexpected status");
        STASIS_ASSERT(task->signaled_by == test->expected_signal, "Unexpected signal");
        STASIS_ASSERT(task->pid == MP_POOL_PID_UNUSED, "Unexpected PID. Should be marked UNUSED.");
        mp_pool_show_summary(p);
        mp_pool_free(&p);
    }
}

void test_mp_fail_fast() {
    char *commands_ff[] = {
        "sleep 1; true",
        "sleep 2; true",
        "sleep 3; false",
        "sleep 10; true", // these...
        "sleep 10; true", // shouldn't...
        "sleep 10; true", // execute... but need to be here to satisfy the signaled_by test below
    };

    struct MultiProcessingPool *p;
    STASIS_ASSERT((p = mp_pool_init("failfast", "failfastlogs")) != NULL, "Failed to initialize pool");
    for (size_t i = 0; i < sizeof(commands_ff) / sizeof(*commands_ff); i++) {
        struct MultiProcessingTask *task;
        char *command = commands_ff[i];
        STASIS_ASSERT(mp_pool_task(p, "task", NULL, (char *) command) != NULL, "Failed to queue task");
    }

    STASIS_ASSERT(mp_pool_join(p, get_cpu_count(), MP_POOL_FAIL_FAST) < 0, "Unexpected result");

    struct result {
        int total_signaled;
        int total_status;
        int total_unused;
    } result = {
        .total_signaled = 0,
        .total_status = 0,
        .total_unused = 0,
    };
    for (size_t i = 0; i < p->num_used; i++) {
        struct MultiProcessingTask *task = &p->task[i];
        if (task->signaled_by) result.total_signaled++;
        if (task->status) result.total_status++;
        if (task->pid == MP_POOL_PID_UNUSED) result.total_unused++;
    }
    STASIS_ASSERT(result.total_status == 1, "Unexpected status count");
    STASIS_ASSERT(result.total_signaled == 3, "Unexpected signal count");
    STASIS_ASSERT(result.total_unused == 5, "Unexpected PID. Should be marked UNUSED.");
    mp_pool_show_summary(p);
    mp_pool_free(&p);
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static void *pool_container(void *data) {
    char *commands_sc[] = {
        "sleep 10; echo done sleeping"
    };
    struct MultiProcessingPool **x = (struct MultiProcessingPool **) data;
    struct MultiProcessingPool *p = (*x);
    pthread_mutex_lock(&mutex);
    mp_pool_task(p, "stop_resume_test", NULL, commands_sc[0]);
    mp_pool_join(p, 1, 0);
    mp_pool_show_summary(p);
    mp_pool_free(&p);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void test_mp_stop_continue() {
    struct MultiProcessingPool *p = NULL;
    STASIS_ASSERT((p = mp_pool_init("stopcontinue", "stopcontinuelogs")) != NULL, "Failed to initialize pool");
    pthread_t th;
    pthread_create(&th, NULL, pool_container, &p);
    sleep(2);
    if (p->task[0].pid != MP_POOL_PID_UNUSED) {
        STASIS_ASSERT(kill(p->task[0].pid, SIGSTOP) == 0, "SIGSTOP failed");
        sleep(2);
        STASIS_ASSERT(kill(p->task[0].pid, SIGCONT) == 0, "SIGCONT failed");
    } else {
        STASIS_ASSERT(false, "Task was marked as unused when it shouldn't have been");
    }
    pthread_join(th, NULL);
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_mp_pool_init,
        test_mp_task,
        test_mp_pool_join,
        test_mp_pool_free,
        test_mp_pool_workflow,
        test_mp_fail_fast,
        test_mp_stop_continue
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}
