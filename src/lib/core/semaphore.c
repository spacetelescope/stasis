/**
* @file semaphore.c
*/
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "core_message.h"
#include "sem.h"
#include "utils.h"

int semaphore_created_by_this_process = 0;

int semaphore_init(struct Semaphore **s, const char *name, const int value) {
    if (*s == NULL) {
        *s = mmap(NULL, sizeof(struct Semaphore), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
        if (*s == MAP_FAILED) {
            SYSERROR("mmap() failed");
            exit(1);
        }
    }
#if defined(STASIS_OS_DARWIN)
    (*s)->sem = dispatch_semaphore_create(value);
    // see: dispatch_semaphore_create
#else
    if (sem_init(&(*s)->sem, 1, value)) {
        SYSERROR("sem_init() failed: %s", strerror(errno));
        exit(1);
    }
    // see: sem_init(3)
#endif
    snprintf((*s)->name, STASIS_NAME_MAX, "%s", name);
    SYSDEBUG("%s initialized", (*s)->name);

    return 0;
}

int semaphore_wait(struct Semaphore *s) {
#if defined(STASIS_SEMAPHORE_DEBUG)
    int sgv_value = 0;
    int sgv_ret = sem_getvalue(s->sem, &sgv_value);
    SYSDEBUG("sem_getvalue() returned %d, value %d", sgv_ret, sgv_value);
#endif
#if defined(STASIS_OS_DARWIN)
    const int status = dispatch_semaphore_wait(s->sem, DISPATCH_TIME_FOREVER);
#else
    const int status = sem_wait(&s->sem);
#endif
#if defined(STASIS_SEMAPHORE_DEBUG)
    SYSDEBUG("returning %d", status);
#endif
    return status;
}

int semaphore_post(struct Semaphore *s) {
#if defined(STASIS_SEMAPHORE_DEBUG)
    int sgv_value = 0;
    int sgv_ret = sem_getvalue(s->sem, &sgv_value);
    SYSDEBUG("sem_getvalue() returned %d, value %d", sgv_ret, sgv_value);
#endif
#if defined(STASIS_OS_DARWIN)
    const int status = dispatch_semaphore_signal(s->sem);
#else
    const int status = sem_post(&s->sem);
#endif
#if defined(STASIS_SEMAPHORE_DEBUG)
    SYSDEBUG("returning %d", status);
#endif
    return status;
}

void semaphore_destroy(struct Semaphore **s) {
#if defined(STASIS_OS_DARWIN)
    dispatch_release((*s)->sem);
#endif
    if (*s) {
        memset(&(*s)->sem, 0, sizeof((*s)->sem));
        (*s)->name[0] = '\0';
        munmap(*s, sizeof(**s));
    }
}