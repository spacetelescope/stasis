/**
* @file semaphore.c
*/
#include <stdio.h>
#include <fcntl.h>

#include "core_message.h"
#include "sem.h"
#include "utils.h"

struct Semaphore *semaphores[1000] = {0};
bool semaphore_handle_exit_ready = false;

void semaphore_handle_exit() {
    for (size_t i = 0; i < sizeof(semaphores) / sizeof(*semaphores); ++i) {
        if (semaphores[i]) {
            SYSDEBUG("%s", semaphores[i]->name);
            semaphore_destroy(semaphores[i]);
        }
    }
}

static void register_semaphore(struct Semaphore *s) {
    struct Semaphore **cur = semaphores;
    size_t i = 0;
    while (i < sizeof(semaphores) / sizeof(*semaphores) && cur != NULL) {
        cur++;
        i++;
    }
    cur = &s;
}

int semaphore_init(struct Semaphore *s, const char *name, const int value) {
#if defined(STASIS_OS_DARWIN)
    // see: sem_open(2)
    const size_t max_namelen = PSEMNAMLEN;
#else
    // see: sem_open(3)
    const size_t max_namelen = STASIS_NAME_MAX;
#endif
    snprintf(s->name, max_namelen, "/%s", name);
    s->sem = sem_open(s->name, O_CREAT, 0644, value);
    if (s->sem == SEM_FAILED) {
        return -1;
    }
    SYSDEBUG("%s", s->name);
    register_semaphore(s);
    if (!semaphore_handle_exit_ready) {
        atexit(semaphore_handle_exit);
    }

    return 0;
}

int semaphore_wait(struct Semaphore *s) {
    return sem_wait(s->sem);
}

int semaphore_post(struct Semaphore *s) {
    return sem_post(s->sem);
}

void semaphore_destroy(struct Semaphore *s) {
    if (!s) {
        SYSDEBUG("%s", "would have crashed");
        return;
    }
    SYSDEBUG("%s", s->name);
    sem_close(s->sem);
    sem_unlink(s->name);
}