#include <stdio.h>
#include <fcntl.h>
#include "sem.h"

int semaphore_init(struct Semaphore *s, const char *name, const int value) {
    snprintf(s->name, sizeof(s->name), "/%s", name);
    s->sem = sem_open(s->name, O_CREAT, 0644, value);
    if (s->sem == SEM_FAILED) {
        perror("sem_open");
        return -1;
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
    sem_close(s->sem);
    sem_unlink(s->name);
}
