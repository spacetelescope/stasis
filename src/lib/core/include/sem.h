#ifndef STASIS_SEMAPHORE_H
#define STASIS_SEMAPHORE_H

#include <semaphore.h>

struct Semaphore {
    sem_t *sem;
    char name[255];
};

int semaphore_init(struct Semaphore *s, const char *name, int value);
int semaphore_wait(struct Semaphore *s);
int semaphore_post(struct Semaphore *s);

void semaphore_destroy(struct Semaphore *s);

#endif //STASIS_SEMAPHORE_H