/**
* @file sem.h
*/
#ifndef STASIS_SEMAPHORE_H
#define STASIS_SEMAPHORE_H

#include "core.h"
#include <semaphore.h>

struct Semaphore {
    sem_t *sem;
    char name[STASIS_NAME_MAX];
};

/**
 * Initialize a cross-platform semaphore (Linux/Darwin)
 *
 * @code c
 * #include "sem.h"
 *
 * int main(int argc, char *argv[]) {
 *     struct Semaphore s;
 *     if (semaphore_init(&s, "mysem", 1)) {
 *         perror("semaphore_init failed");
 *         exit(1);
 *     }
 *     if (semaphore_wait(&s)) {
 *         perror("semaphore_wait failed");
 *         exit(1);
 *     }
 *
 *     //
 *     // Critical section
 *     // CODE HERE
 *     //
 *
 *     if (semaphore_post(&s)) {
 *         perror("semaphore_post failed");
 *         exit(1);
 *     }
 * }
 * @endcode
 *
 * @param s a pointer to `Semaphore`
 * @param name of the semaphore
 * @param value initial value of the semaphore
 * @return -1 on error
 * @return 0 on success
 */
int semaphore_init(struct Semaphore *s, const char *name, int value);
int semaphore_wait(struct Semaphore *s);
int semaphore_post(struct Semaphore *s);

void semaphore_destroy(struct Semaphore *s);

#endif //STASIS_SEMAPHORE_H