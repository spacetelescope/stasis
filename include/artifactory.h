//! @file artifactory.h
#ifndef OMC_ARTIFACTORY_H
#define OMC_ARTIFACTORY_H

#include <stdio.h>
#include <stdlib.h>
#include "omc.h"

//! JFrog Artifactory Authentication struct
struct JFRT_Auth {
    bool insecure_tls; //!< Disable TLS
    char *access_token; //!< Generated access token
    char *password; //!< Password
    char *client_cert_key_path; //!< Path to where SSL key is stored
    char *client_cert_path; //!< Path to where SSL cert is stored
    char *ssh_key_path; //!< Path to SSH private key
    char *ssh_passphrase; //!< Passphrase for SSH private key
    char *user; //!< Account to authenticate as
    char *server_id; //!< Artifactory server identification (unused)
    char *url; //!< Artifactory server address
};

//! JFrog Artifactory Upload struct
struct JFRT_Upload {
    bool quiet; //!< Enable quiet mode
    char *project; //!< Destination project name
    bool ant; //!< Enable Ant style regex
    bool archive; //!< Generate a ZIP archive of the uploaded file(s)
    char *build_name; //!< Build name
    char *build_number; //!< Build number
    bool deb; //!< Is Debian package?
    bool detailed_summary; //!< Enable upload summary
    bool dry_run; //!< Enable dry run (no-op)
    char *exclusions; //!< Exclude patterns (separated by semicolons)
    bool explode; //!< If uploaded file is an archive, extract it at the destination
    bool fail_no_op; //!< Exit 2 when no file are affected
    bool flat; //!< Upload with exact file system structure
    bool include_dirs; //!< Enable to upload empty directories
    char *module; //!< Build-info module name (optional)
    bool recursive; //!< Upload files recursively
    bool regexp; //!< Use regular expressions instead of wildcards
    int retries; //!< Number of retries before giving up
    int retry_wait_time; //!< Seconds between retries
    char *spec; //!< Path to JSON upload spec
    char *spec_vars;
    bool symlinks; //!< Preserve symbolic links
    bool sync_deletes; //!< Destination is replaced by uploaded files
    char *target_props; //!< Properties (separated by semicolons)
    int threads; //!< Thread count
    bool workaround_parent_only; //!< Change directory to local parent directory before uploading files
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

/**
 * Download the JFrog CLI tool from jfrog.com
 * ```c
 * if (artifactory_download_cli(".",
 *         "https://releases.jfrog.io/artifactory",
 *         "jfrog-cli",
 *         "v2-jf",
 *         "[RELEASE]",
 *         "Linux",
 *         "x86_64",
 *         "jf") {
 *     remove("./jf");
 *     fprintf(stderr, "Failed to download JFrog CLI\n");
 *     exit(1);
 * }
 *
 * ```
 *
 * @param dest Directory path
 * @param jfrog_artifactory_base_url jfrog.com base URL
 * @param jfrog_artifactory_product jfrog.com project (jfrog-cli)
 * @param cli_major_ver Version series (v1, v2-jf, vX-jf)
 * @param version Version to download. "[RELEASE]" will download the latest version available
 * @param os Operating system name
 * @param arch System CPU architecture
 * @param remote_filename File to download (jf)
 * @return
 */
int artifactory_download_cli(char *dest,
                             char *jfrog_artifactory_base_url,
                             char *jfrog_artifactory_product,
                             char *cli_major_ver,
                             char *version,
                             char *os,
                             char *arch,
                             char *remote_filename);

/**
 * JFrog CLI binding. Executes the "jf" tool with arguments.
 *
 * ```c
 * struct JFRT_Auth auth_ctx;
 * auth_ctx.user = strdup("myuser");
 * auth_ctx.password = strdup("mypassword");
 * auth_ctx.url = strdup("https://myserver.tld/artifactory");
 * jfrt_auth_init(&auth_ctx);
 *
 * if (jfrog_cli(&auth_ctx, "ping") {
 *     fprintf(stderr, "Failed to ping artifactory server: %s\n", auth_ctx.url);
 *     exit(1);
 * }
 * ```
 *
 * @param auth JFRT_Auth structure
 * @param args Command line arguments to pass to "jf" tool
 * @return exit code from "jf"
 */
int jfrog_cli(struct JFRT_Auth *auth, char *args);

/**
 * Issue an Artifactory server ping
 *
 * ```c
 * struct JFRT_Auth auth_ctx;
 * auth_ctx.user = strdup("myuser");
 * auth_ctx.password = strdup("mypassword");
 * auth_ctx.url = strdup("https://myserver.tld/artifactory");
 * jfrt_auth_init(&auth_ctx);
 *
 * if (jfrog_cli_ping(&auth_ctx)) {
 *     fprintf(stderr, "Failed to ping artifactory server: %s\n", auth_ctx.url);
 *     exit(1);
 * }
 * ```
 *
 * @param auth JFRT_Auth structure
 * @return exit code from "jf"
 */
int jfrog_cli_rt_ping(struct JFRT_Auth *auth);

/**
 * Upload files to an Artifactory repository
 *
 * ```c
 * struct JFRT_Auth auth_ctx;
 * auth_ctx.user = strdup("myuser");
 * auth_ctx.password = strdup("mypassword");
 * auth_ctx.url = strdup("https://myserver.tld/artifactory");
 * jfrt_auth_init(&auth_ctx);
 *
 * struct JFRT_Upload upload_ctx;
 * jfrt_upload_init(&upload_ctx);
 *
 * if (jfrt_cli_rt_upload(&auth_ctx, &upload_ctx,
 *     "local/files_*.ext", "repo_name/ext_files/")) {
 *     fprintf(stderr, "Upload failed\n");
 *     exit(1);
 * }
 * ```
 *
 * @param auth JFRT_Auth structure
 * @param ctx JFRT_Upload structure
 * @param src local pattern to upload
 * @param repo_path remote Artifactory destination path
 * @return exit code from "jf"
 */
int jfrog_cli_rt_upload(struct JFRT_Auth *auth, struct JFRT_Upload *ctx, char *src, char *repo_path);

/**
 * Download a file from an Artifactory repository
 *
 * ```c
 * struct JFRT_Auth auth_ctx;
 * auth_ctx.user = strdup("myuser");
 * auth_ctx.password = strdup("mypassword");
 * auth_ctx.url = strdup("https://myserver.tld/artifactory");
 * jfrt_auth_init(&auth_ctx);
 *
 * struct JFRT_Download download_ctx;
 * memset(download_ctx, 0, sizeof(download_ctx));
 *
 * if (jfrt_cli_rt_download(&auth_ctx, &download_ctx,
 *     "repo_name/ext_files/", "local/files_*.ext")) {
 *     fprintf(stderr, "Upload failed\n");
 *     exit(1);
 * }
 * ```
 *
 * @param auth JFRT_Auth structure
 * @param ctx  JFRT_Download structure
 * @param repo_path Remote repository w/ file pattern
 * @param dest Local destination path
 * @return exit code from "jf"
 */
int jfrog_cli_rt_download(struct JFRT_Auth *auth, struct JFRT_Download *ctx, char *repo_path, char *dest);

/**
 * Collect runtime data for Artifactory build object.
 *
 * ```c
 * struct JFRT_Auth auth_ctx;
 * auth_ctx.user = strdup("myuser");
 * auth_ctx.password = strdup("mypassword");
 * auth_ctx.url = strdup("https://myserver.tld/artifactory");
 * jfrt_auth_init(&auth_ctx);
 *
 * if (jfrog_cli_rt_build_collect_env(&auth_ctx, "mybuildname", "1.2.3+gabcdef")) {
 *     fprintf(stderr, "Failed to collect runtime data for Artifactory build object\n");
 *     exit(1);
 * }
 * ```
 *
 * @param auth JFRT_Auth structure
 * @param build_name Artifactory build name
 * @param build_number Artifactory build number
 * @return exit code from "jf"
 */
int jfrog_cli_rt_build_collect_env(struct JFRT_Auth *auth, char *build_name, char *build_number);

/**
 * Publish build object to Artifactory server
 *
 * ```c
 * struct JFRT_Auth auth_ctx;
 * auth_ctx.user = strdup("myuser");
 * auth_ctx.password = strdup("mypassword");
 * auth_ctx.url = strdup("https://myserver.tld/artifactory");
 * jfrt_auth_init(&auth_ctx);
 *
 * if (jfrog_cli_rt_build_collect_env(&auth_ctx, "mybuildname", "1.2.3+gabcdef")) {
 *     fprintf(stderr, "Failed to collect runtime data for Artifactory build object\n");
 *     exit(1);
 * }
 *
 * if (jfrog_cli_rt_build_publish(&auth_ctx, "mybuildname", "1.2.3+gabcdef")) {
 *     fprintf(stderr, "Failed to publish Artifactory build object\n");
 *     exit(1);
 * }
 * ```
 *
 * @param auth JFRT_Auth structure
 * @param build_name Artifactory build name
 * @param build_number Artifactory build number
 * @return exit code from "jf"
 */
int jfrog_cli_rt_build_publish(struct JFRT_Auth *auth, char *build_name, char *build_number);

/**
 * Configure JFrog CLI authentication according to OMC specs
 *
 * This function will use the OMC_JF_* environment variables to configure the authentication
 * context. With this in mind, if an OMC_JF_* environment variable is not defined, the original value of
 * the structure member will be used instead.
 *
 * Use OMC_JF_* variables to configure context
 *
 * ```c
 * struct JFRT_Auth auth_ctx;
 * jfrt_auth_init(&ctx);
 * ```
 *
 * Use your own input, but let the environment take over when variables are defined
 *
 * ```c
 * struct JFRT_Auth auth_ctx;
 * auth_ctx.user = strdup("myuser");
 * auth_ctx.password = strdup("mypassword");
 * auth_ctx.url = strdup("https://myserver.tld/artifactory");
 * jfrt_auth_init(&auth_ctx);
 * ```
 *
 * Use your own input without OMC's help. Purely an illustrative example.
 *
 * ```c
 * struct JFRT_Auth auth_ctx;
 * memset(auth_ctx, 0, sizeof(auth_ctx));
 * auth_ctx.user = strdup("myuser");
 * auth_ctx.password = strdup("mypassword");
 * auth_ctx.url = strdup("https://myserver.tld/artifactory");
 * ```
 *
 * @param auth_ctx
 * @return
 */
int jfrt_auth_init(struct JFRT_Auth *auth_ctx);

/**
 * Zero-out and apply likely defaults to a JFRT_Upload structure
 * @param ctx JFRT_Upload structure
 */
void jfrt_upload_init(struct JFRT_Upload *ctx);

#endif //OMC_ARTIFACTORY_H