//! @file github.h
#ifndef STASIS_GITHUB_H
#define STASIS_GITHUB_H

#include <curl/curl.h>

#define STASIS_GITHUB_API_VERSION "2022-11-28"

char *get_git_tag(const char *repopath, const char *ref, unsigned parent);
char *get_git_commit_root(const char *repo_path, const char *ref);
char *get_git_describe(const char *repopath, const char *ref);
int get_github_release_notes(const char *api_token, const char *repo, const char *tag, const char *target_commitish, char **output);
int get_git_release_notes(const char *repo_path, const char *tag, const char *target_commitish, char **output);


#endif //STASIS_GITHUB_H