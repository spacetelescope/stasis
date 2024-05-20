//
// Created by jhunk on 5/19/2024.
//

#ifndef OMC_OS_WINDOWS_H
#define OMC_OS_WINDOWS_H


#include <winsock2.h>
#include <windows.h>
#include <direct.h>
#define lstat stat
#define NAME_MAX 256
#define MAXNAMLEN 256
#define __environ _environ
#undef mkdir
#define mkdir(X, Y) _mkdir(X)

int setenv(const char *key, const char *value, int overwrite);
char *realpath(const char *path, char **dest);

#endif //OMC_OS_WINDOWS_H
