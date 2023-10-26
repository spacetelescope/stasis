//
// Created by jhunk on 10/4/23.
//

#ifndef OHMYCAL_SYSTEM_H
#define OHMYCAL_SYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/stat.h>

struct Process {
    char stdout[PATH_MAX];
    char stderr[PATH_MAX];
    int redirect_stderr;
    int returncode;
};

int shell(struct Process *proc, char *args[]);
int shell2(struct Process *proc, char *args);
int shell_safe(struct Process *proc, char *args[]);

#endif //OHMYCAL_SYSTEM_H
