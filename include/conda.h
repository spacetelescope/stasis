//
// Created by jhunk on 5/14/23.
//

#ifndef OHMYCAL_CONDA_H
#define OHMYCAL_CONDA_H

#include <stdio.h>
#include <string.h>
#include "ohmycal.h"

#define CONDA_INSTALL_PREFIX "conda"

int python_exec(const char *args);
int pip_exec(const char *args);
int conda_exec(const char *args);
int conda_activate(const char *root, const char *env_name);
void conda_env_create_from_uri(char *name, char *uri);
void conda_env_create(char *name, char *python_version, char *packages);
void conda_env_remove(char *name);
void conda_env_export(char *name, char *output_dir, char *output_filename);
int conda_index(const char *path);
#endif //OHMYCAL_CONDA_H
