/**
 * @file relocation.h
 */
#ifndef OMC_RELOCATION_H
#define OMC_RELOCATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <unistd.h>

void replace_text(char *original, const char *target, const char *replacement);
void file_replace_text(const char* filename, const char* target, const char* replacement);

#endif //OMC_RELOCATION_H
