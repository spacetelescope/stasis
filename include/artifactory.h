#ifndef OMC_ARTIFACTORY_H
#define OMC_ARTIFACTORY_H

#include <stdio.h>
#include <stdlib.h>
#include "omc.h"

struct JFRT_Auth {
    bool insecure_tls;
    char *access_token;
    char *password;
    char *client_cert_key_path;
    char *client_cert_path;
    char *ssh_key_path;
    char *ssh_passphrase;
    char *user;
    char *server_id;
    char *url;
};

struct JFRT_Upload {
    bool quiet;
    char *project;
    bool ant;
    bool archive;
    char *build_name;
    char *build_number;
    bool deb;
    bool detailed_summary;
    bool dry_run;
    char *exclusions;
    bool explode;
    bool fail_no_op;
    bool flat;
    bool include_dirs;
    char *module;
    bool recursive;
    bool regexp;
    int retries;
    int retry_wait_time;
    char *spec;
    char *spec_vars;
    bool symlinks;
    bool sync_deletes;
    char *target_props;
    int threads;
    bool workaround_parent_only;
};

struct JFRT_Download {
    char *archive_entries;
    char *build;
    char *build_name;
    char *build_number;
    char *bundle;
    bool detailed_summary;
    bool dry_run;
    char *exclude_artifacts;
    char *exclude_props;
    char *exclusions;
    bool explode;
    bool fail_no_op;
    bool flat;
    char *gpg_key;
    char *include_deps;
    char *include_dirs;
    int limit;
    int min_split;
    char *module;
    int offset;
    char *project;
    char *props;
    bool quiet;
    bool recursive;
    int retries;
    int retry_wait_time;
    bool skip_checksum;
    char *sort_by;
    char *sort_order;
    char *spec;
    char *spec_vars;
    int split_count;
    bool sync_deletes;
    int threads;
    bool validate_symlinks;
};

int artifactory_download_cli(char *dest,
                             char *jfrog_artifactory_base_url,
                             char *jfrog_artifactory_product,
                             char *cli_major_ver,
                             char *version,
                             char *os,
                             char *arch,
                             char *remote_filename);
int jfrog_cli(struct JFRT_Auth *auth, char *args);
int jfrog_cli_rt_ping(struct JFRT_Auth *auth);
int jfrog_cli_rt_upload(struct JFRT_Auth *auth, struct JFRT_Upload *ctx, char *src, char *repo_path);
int jfrog_cli_rt_download(struct JFRT_Auth *auth, struct JFRT_Download *ctx, char *repo_path, char *dest);
int jfrog_cli_rt_build_collect_env(struct JFRT_Auth *auth, char *build_name, char *build_number);
int jfrog_cli_rt_build_publish(struct JFRT_Auth *auth, char *build_name, char *build_number);
void jfrt_upload_set_defaults(struct JFRT_Upload *ctx);

#endif //OMC_ARTIFACTORY_H
