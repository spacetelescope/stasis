//! @file conda.h
#ifndef STASIS_CONDA_H
#define STASIS_CONDA_H

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include "core.h"
#include "download.h"

#define CONDA_INSTALL_PREFIX "conda"
#define PYPI_INDEX_DEFAULT "https://pypi.org/simple"

#define PKG_USE_PIP 0
#define PKG_USE_CONDA 1

#define PKG_NOT_FOUND 0
#define PKG_FOUND 1

#define PKG_INDEX_PROVIDES_ERROR_MESSAGE_OFFSET (-10)
#define PKG_E_SUCCESS (PKG_INDEX_PROVIDES_ERROR_MESSAGE_OFFSET + 0)
#define PKG_INDEX_PROVIDES_E_INTERNAL_MODE_UNKNOWN (PKG_INDEX_PROVIDES_ERROR_MESSAGE_OFFSET + 1)
#define PKG_INDEX_PROVIDES_E_INTERNAL_LOG_HANDLE (PKG_INDEX_PROVIDES_ERROR_MESSAGE_OFFSET + 2)
#define PKG_INDEX_PROVIDES_E_MANAGER_RUNTIME (PKG_INDEX_PROVIDES_ERROR_MESSAGE_OFFSET + 3)
#define PKG_INDEX_PROVIDES_E_MANAGER_SIGNALED (PKG_INDEX_PROVIDES_ERROR_MESSAGE_OFFSET + 4)
#define PKG_INDEX_PROVIDES_E_MANAGER_EXEC (PKG_INDEX_PROVIDES_ERROR_MESSAGE_OFFSET + 5)
#define PKG_INDEX_PROVIDES_FAILED(ECODE) ((ECODE) <= PKG_INDEX_PROVIDES_ERROR_MESSAGE_OFFSET)

struct MicromambaInfo {
    char *micromamba_prefix;    //!< Path to write micromamba binary
    char *conda_prefix;         //!< Path to install conda base tree
};

/**
 * Execute micromamba
 * @param info MicromambaInfo data structure (must be populated before use)
 * @param command printf-style formatter string
 * @param ... variadic arguments
 * @return exit code
 */
int micromamba(struct MicromambaInfo *info, char *command, ...);

/**
 * Execute Python
 * Python interpreter is determined by PATH
 *
 * ```c
 * if (python_exec("-c 'printf(\"Hello world\")'")) {
 *     fprintf(stderr, "Hello world failed\n");
 *     exit(1);
 * }
 * ```
 *
 * @param args arguments to pass to interpreter
 * @return exit code from python interpreter
 */
int python_exec(const char *args);

/**
 * Execute Pip
 * Pip is determined by PATH
 *
 * ```c
 * if (pip_exec("freeze")) {
 *     fprintf(stderr, "pip freeze failed\n");
 *     exit(1);
 * }
 * ```
 *
 * @param args arguments to pass to Pip
 * @return exit code from Pip
 */
int pip_exec(const char *args);

/**
 * Execute conda (or if possible, mamba)
 * Conda/Mamba is determined by PATH
 *
 * ```c
 * if (conda_exec("env list")) {
 *     fprintf(stderr, "Failed to list conda environments\n");
 *     exit(1);
 * }
 * ```
 *
 * @param args arguments to pass to Conda
 * @return exit code from Conda
 */
int conda_exec(const char *args);

/**
 * Configure the runtime environment to use Conda/Mamba
 *
 * ```c
 * if (conda_activate("/path/to/conda/installation", "base")) {
 *     fprintf(stderr, "Failed to activate conda's base environment\n");
 *     exit(1);
 * }
 * ```
 *
 * @param root directory where conda is installed
 * @param env_name the conda environment to activate
 * @return 0 on success, -1 on error
 */
int conda_activate(const char *root, const char *env_name);

/**
 * Configure the active conda installation for headless operation
 */
int conda_setup_headless();

/**
 * Creates a Conda environment from a YAML config
 *
 * ```c
 * if (conda_env_create_from_uri("myenv", "https://myserver.tld/environment.yml")) {
 *     fprintf(stderr, "Environment creation failed\n");
 *     exit(1);
 * }
 * ```
 *
 * @param name Name of new environment to create
 * @param uri /path/to/environment.yml
 * @param uri file:///path/to/environment.yml
 * @param uri http://myserver.tld/environment.yml
 * @param uri https://myserver.tld/environment.yml
 * @param uri ftp://myserver.tld/environment.yml
 * @return exit code from "conda"
 */
int conda_env_create_from_uri(char *name, char *uri);

/**
 * Create a Conda environment using generic package specs
 *
 * ```c
 * // Create a basic environment without any conda packages
 * if (conda_env_create("myenv", "3.11", NULL)) {
 *     fprintf(stderr, "Environment creation failed\n");
 *     exit(1);
 * }
 *
 * // Create a basic environment and install conda packages
 * if (conda_env_create("myenv", "3.11", "hstcal fitsverify")) {
 *     fprintf(stderr, "Environment creation failed\n");
 *     exit(1);
 * }
 * ```
 *
 * @param name Environment name
 * @param python_version Desired version of Python
 * @param packages Packages to install (or NULL)
 * @return exit code from "conda"
 */
int conda_env_create(char *name, char *python_version, char *packages);

/**
 * Remove a Conda environment
 *
 * ```c
 * if (conda_env_remove("myenv")) {
 *     fprintf(stderr, "Unable to remove conda environment\n");
 *     exit(1);
 * }
 * ```
 *
 * @param name Environment name
 * @return exit code from "conda"
 */
int conda_env_remove(char *name);

/**
 * Export a Conda environment in YAML format
 *
 * ```c
 * if (conda_env_export("myenv", "./", "myenv.yml")) {
 *     fprintf(stderr, "Unable to export environment\n");
 *     exit(1);
 * }
 * ```
 *
 * @param name Environment name to export
 * @param output_dir Destination directory
 * @param output_filename Destination file name
 * @return exit code from "conda"
 */
int conda_env_export(char *name, char *output_dir, char *output_filename);

/**
 * Run "conda index" on a local conda channel
 *
 * ```c
 * if (conda_index("/path/to/channel")) {
 *     fprintf(stderr, "Unable to index requested path\n");
 *     exit(1);
 * }
 * ```
 *
 * @param path Top-level directory of conda channel
 * @return exit code from "conda"
 */
int conda_index(const char *path);

/**
 * Determine whether a package index contains a package
 *
 * ```c
 * int result = pkg_index_provides(USE_PIP, NULL, "numpy>1.26");
 * if (PKG_INDEX_PROVIDES_FAILED(result)) {
 *     fprintf(stderr, "failed: %s\n", pkg_index_provides_strerror(result));
 *     exit(1);
 * } else if (result == PKG_NOT_FOUND) {
 *     // package does not exist upstream
 * } else {
 *     // package exists upstream
 * }
 * ```
 *
 * @param mode USE_PIP
 * @param mode USE_CONDA
 * @param index a file system path or url pointing to a simple index or conda channel
 * @param spec a pip package specification (e.g. `name==1.2.3`)
 * @param spec a conda package specification (e.g. `name=1.2.3`)
 * @return PKG_NOT_FOUND, if not found
 * @return PKG_FOUND, if found
 * @return PKG_E_INDEX_PROVIDES_{ERROR}, on error (see conda.h)
 */
int pkg_index_provides(int mode, const char *index, const char *spec);
const char *pkg_index_provides_strerror(int code);

char *conda_get_active_environment();

int conda_env_exists(const char *root, const char *name);

#endif //STASIS_CONDA_H
