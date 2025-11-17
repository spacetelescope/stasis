/**
* @file semaphore.c
*/
#include <stdio.h>
#include <fcntl.h>

#include "core_message.h"
#include "sem.h"
#include "utils.h"

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
    return 0;
}

int semaphore_wait(struct Semaphore *s) {
    SYSDEBUG("%s", s->name);
    return sem_wait(s->sem);
}

int semaphore_post(struct Semaphore *s) {
    SYSDEBUG("%s", s->name);
    return sem_post(s->sem);
}

void semaphore_destroy(struct Semaphore *s) {
    SYSDEBUG("%s", s->name);
    sem_close(s->sem);
    sem_unlink(s->name);
}
