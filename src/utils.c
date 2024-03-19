#include <stdarg.h>
#include "omc.h"

extern struct OMC_GLOBAL globals;

char *dirstack[OMC_DIRSTACK_MAX];
const ssize_t dirstack_max = sizeof(dirstack) / sizeof(dirstack[0]);
ssize_t dirstack_len = 0;

int pushd(const char *path) {
    if (dirstack_len + 1 > dirstack_max) {
        return -1;
    }
    dirstack[dirstack_len] = realpath(".", NULL);
    dirstack_len++;
    return chdir(path);
}

int popd() {
    int result = -1;
    if (dirstack_len - 1 < 0) {
        return result;
    }
    dirstack_len--;
    result = chdir(dirstack[dirstack_len]);
    guard_free(dirstack[dirstack_len]);
    return result;
}

int rmtree(char *_path) {
    int status = 0;
    char path[PATH_MAX] = {0};
    strncpy(path, _path, sizeof(path));
    DIR *dir;
    struct dirent *d_entity;

    dir = opendir(path);
    if (!dir) {
        return 1;
    }

    while ((d_entity = readdir(dir)) != NULL) {
        char abspath[PATH_MAX] = {0};
        strcat(abspath, path);
        strcat(abspath, DIR_SEP);
        strcat(abspath, d_entity->d_name);

        if (!strcmp(d_entity->d_name, ".") || !strcmp(d_entity->d_name, "..") || !strcmp(abspath, path)) {
            continue;
        }

        // Test for sufficient privilege
        if (access(abspath, F_OK) < 0 && errno == EACCES) {
            continue;
        }

        // Push directories on to the stack first
        if (d_entity->d_type == DT_DIR) {
            rmtree(abspath);
        } else {
            remove(abspath);
        }
    }
    closedir(dir);

    if (access(path, F_OK) == 0) {
        remove(path);
    }
    return status;
}

char *expandpath(const char *_path) {
    if (_path == NULL) {
        return NULL;
    }
    const char *homes[] = {
            "HOME",
            "USERPROFILE",
    };
    char home[PATH_MAX];
    char tmp[PATH_MAX];
    char *ptmp = tmp;
    char result[PATH_MAX];
    char *sep = NULL;

    memset(home, '\0', sizeof(home));
    memset(ptmp, '\0', sizeof(tmp));
    memset(result, '\0', sizeof(result));

    strncpy(ptmp, _path, PATH_MAX - 1);

    // Check whether there's a reason to continue processing the string
    if (*ptmp != '~') {
        return strdup(ptmp);
    }

    // Remove tilde from the string and shift its contents to the left
    strchrdel(ptmp, "~");

    // Figure out where the user's home directory resides
    for (size_t i = 0; i < sizeof(homes) / sizeof(*homes); i++) {
        char *tmphome;
        if ((tmphome = getenv(homes[i])) != NULL) {
            strncpy(home, tmphome, PATH_MAX - 1);
            break;
        }
    }

    // A broken runtime environment means we can't do anything else here
    if (isempty(home)) {
        return NULL;
    }

    // Scan the path for a directory separator
    if ((sep = strpbrk(ptmp, "/\\")) != NULL) {
        // Jump past it
        ptmp = sep + 1;
    }

    // Construct the new path
    strncat(result, home, PATH_MAX - 1);
    if (sep) {
        strncat(result, DIR_SEP, PATH_MAX - 1);
        strncat(result, ptmp, PATH_MAX - 1);
    }

    return strdup(result);
}

char *path_basename(char *path) {
    char *result = NULL;
    char *last = NULL;

    if ((last = strrchr(path, '/')) == NULL) {
        return path;
    }
    // Perform a lookahead ensuring the string is valid beyond the last separator
    if (last++ != NULL) {
        result = last;
    }

    return result;
}

char *path_dirname(char *path) {
    if (!path) {
        return "";
    }
    if (strlen(path) == 1 && *path == '/') {
        return "/";
    }
    char *pos = strrchr(path, '/');
    if (!pos) {
        return ".";
    }
    *path = '\0';

    return path;
}

char **file_readlines(const char *filename, size_t start, size_t limit, ReaderFn *readerFn) {
    FILE *fp = NULL;
    char **result = NULL;
    char *buffer = NULL;
    size_t lines = 0;
    int use_stdin = 0;

    if (strcmp(filename, "-") == 0) {
        use_stdin = 1;
    }

    if (use_stdin) {
        fp = stdin;
    } else {
        fp = fopen(filename, "r");
    }

    if (fp == NULL) {
        perror(filename);
        fprintf(SYSERROR);
        return NULL;
    }

    // Allocate buffer
    if ((buffer = calloc(OMC_BUFSIZ, sizeof(char))) == NULL) {
        perror("line buffer");
        fprintf(SYSERROR);
        if (!use_stdin) {
            fclose(fp);
        }
        return NULL;
    }

    // count number the of lines in the file
    while ((fgets(buffer, OMC_BUFSIZ - 1, fp)) != NULL) {
        lines++;
    }

    if (!lines) {
        guard_free(buffer);
        if (!use_stdin) {
            fclose(fp);
        }
        return NULL;
    }

    rewind(fp);

    // Handle invalid start offset
    if (start > lines) {
        start = 0;
    }

    // Adjust line count when start offset is non-zero
    if (start != 0 && start < lines) {
        lines -= start;
    }


    // Handle minimum and maximum limits
    if (limit == 0 || limit > lines) {
        limit = lines;
    }

    // Populate results array
    result = calloc(limit + 1, sizeof(char *));
    for (size_t i = start; i < limit; i++) {
        if (i < start) {
            continue;
        }

        if (fgets(buffer, OMC_BUFSIZ - 1, fp) == NULL) {
            break;
        }

        if (readerFn != NULL) {
            int status = readerFn(i - start, &buffer);
            // A status greater than zero indicates we should ignore this line entirely and "continue"
            // A status less than zero indicates we should "break"
            // A zero status proceeds normally
            if (status > 0) {
                i--;
                continue;
            } else if (status < 0) {
                break;
            }
        }
        result[i] = strdup(buffer);
        memset(buffer, '\0', OMC_BUFSIZ);
    }

    guard_free(buffer);
    if (!use_stdin) {
        fclose(fp);
    }
    return result;
}

char *find_program(const char *name) {
    static char result[PATH_MAX] = {0};
    char *_env_path = getenv(PATH_ENV_VAR);
    if (!_env_path) {
        errno = EINVAL;
        return NULL;
    }
    char *path = strdup(_env_path);
    char *path_orig = path;
    char *path_elem = NULL;

    if (!path) {
        errno = ENOMEM;
        return NULL;
    }

    result[0] = '\0';
    while ((path_elem = strsep(&path, PATH_SEP))) {
        char abspath[PATH_MAX] = {0};
        strcat(abspath, path_elem);
        strcat(abspath, DIR_SEP);
        strcat(abspath, name);
        if (access(abspath, F_OK) < 0) {
            continue;
        }
        strncpy(result, abspath, sizeof(result));
        break;
    }
    path = path_orig;
    guard_free(path);
    return strlen(result) ? result : NULL;
}

int touch(const char *filename) {
    if (access(filename, F_OK) == 0) {
        return 0;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror(filename);
        return 1;
    }
    fclose(fp);
    return 0;
}

int git_clone(struct Process *proc, char *url, char *destdir, char *gitref) {
    int result = -1;
    char *chdir_to = NULL;
    char *program = find_program("git");
    if (!program) {
        return result;
    }

    static char command[PATH_MAX];
    sprintf(command, "%s clone --recursive %s", program, url);
    if (destdir && access(destdir, F_OK) < 0) {
        sprintf(command + strlen(command), " %s", destdir);
        result = shell(proc, command);
    }

    if (destdir) {
        chdir_to = destdir;
    } else {
        chdir_to = path_basename(url);
    }

    pushd(chdir_to);
    {
        memset(command, 0, sizeof(command));
        sprintf(command, "%s fetch --all", program);
        result += shell(proc, command);

        if (gitref != NULL) {
            memset(command, 0, sizeof(command));
            sprintf(command, "%s checkout %s", program, gitref);
            result += shell(proc, command);
        }
        popd();
    }
    return result;
}


char *git_describe(const char *path) {
    static char version[NAME_MAX];
    FILE *pp;

    memset(version, 0, sizeof(version));
    if (pushd(path)) {
        return NULL;
    }

    pp = popen("git describe --first-parent --always --tags", "r");
    if (!pp) {
        return NULL;
    }
    fgets(version, sizeof(version) - 1, pp);
    strip(version);
    pclose(pp);
    popd();
    return version;
}

char *git_rev_parse(const char *path, char *args) {
    static char version[NAME_MAX];
    char cmd[PATH_MAX];
    FILE *pp;

    memset(version, 0, sizeof(version));
    if (isempty(args)) {
        fprintf(stderr, "git_rev_parse args cannot be empty\n");
        return NULL;
    }

    if (pushd(path)) {
        return NULL;
    }

    sprintf(cmd, "git rev-parse %s", args);
    pp = popen(cmd, "r");
    if (!pp) {
        return NULL;
    }
    fgets(version, sizeof(version) - 1, pp);
    strip(version);
    pclose(pp);
    popd();
    return version;
}

#define OMC_COLOR_RED "\e[1;91m"
#define OMC_COLOR_GREEN "\e[1;92m"
#define OMC_COLOR_YELLOW "\e[1;93m"
#define OMC_COLOR_BLUE "\e[1;94m"
#define OMC_COLOR_WHITE "\e[1;97m"
#define OMC_COLOR_RESET "\e[0;37m\e[0m"

void msg(unsigned type, char *fmt, ...) {
    FILE *stream = NULL;
    char header[255];
    char status[20];

    if (type & OMC_MSG_NOP) {
        // quiet mode
        return;
    }

    if (!globals.verbose && type & OMC_MSG_RESTRICT) {
        // Verbose mode is not active
        return;
    }

    memset(header, 0, sizeof(header));
    memset(status, 0, sizeof(status));

    va_list args;
    va_start(args, fmt);

    stream = stdout;
    fprintf(stream, "%s", OMC_COLOR_RESET);
    if (type & OMC_MSG_ERROR) {
        // for error output
        stream = stderr;
        fprintf(stream, "%s", OMC_COLOR_RED);
        strcpy(status, " ERROR: ");
    } else if (type & OMC_MSG_WARN) {
        stream = stderr;
        fprintf(stream, "%s", OMC_COLOR_YELLOW);
        strcpy(status, " WARNING: ");
    } else {
        fprintf(stream, "%s", OMC_COLOR_GREEN);
        strcpy(status, " ");
    }

    if (type & OMC_MSG_L1) {
        sprintf(header, "==>%s" OMC_COLOR_RESET OMC_COLOR_WHITE, status);
    } else if (type & OMC_MSG_L2) {
        sprintf(header, " ->%s" OMC_COLOR_RESET, status);
    } else if (type & OMC_MSG_L3) {
        sprintf(header, OMC_COLOR_BLUE "  ->%s" OMC_COLOR_RESET, status);
    }

    fprintf(stream, "%s", header);
    vfprintf(stream, fmt, args);
    fprintf(stream, "%s", OMC_COLOR_RESET);
    va_end(args);
}

void debug_shell() {
    msg(OMC_MSG_L1 | OMC_MSG_WARN, "ENTERING OMC DEBUG SHELL\n" OMC_COLOR_RESET);
    system("/bin/bash -c 'PS1=\"(OMC DEBUG) \\W $ \" bash --norc --noprofile'");
    msg(OMC_MSG_L1 | OMC_MSG_WARN, "EXITING OMC DEBUG SHELL\n" OMC_COLOR_RESET);
    exit(255);
}

char *xmkstemp(FILE **fp, const char *mode) {
    char t_name[PATH_MAX];
    memset(t_name, 0, sizeof(t_name));
    sprintf(t_name, "%s/%s", globals.tmpdir, "OMC.XXXXXX");
    int fd = mkstemp(t_name);
    *fp = fdopen(fd, mode);
    if (!*fp) {
        if (fd > 0)
            close(fd);
        *fp = NULL;
        return NULL;
    }
    char *path = strdup(t_name);
    return path;
}

int isempty_dir(const char *path) {
    DIR *dp;
    struct dirent *rec;
    size_t count = 0;

    dp = opendir(path);
    if (!dp) {
        return -1;
    }
    while ((rec = readdir(dp)) != NULL) {
        if (!strcmp(rec->d_name, ".") || !strcmp(rec->d_name, "..")) {
            continue;
        }
        count++;
    }
    closedir(dp);
    return count == 0;
}

int path_store(char **destptr, size_t maxlen, const char *base, const char *path) {
    char *path_tmp;
    size_t base_len = 0;
    size_t path_len = 0;

    // Both path elements need to be defined to continue
    if (!base || !path) {
        return -1;
    }

    // Initialize destination pointer to length of maxlen
    path_tmp = calloc(maxlen, sizeof(*path_tmp));
    if (!path_tmp) {
        return -1;
    }

    // Ensure generated path will fit in destination
    base_len = strlen(base);
    path_len = strlen(path);
    // 2 = directory separator and NUL terminator
    if (2 + (base_len + path_len) > maxlen) {
        goto l_path_setup_error;
    }

    snprintf(path_tmp, maxlen - 1, "%s/%s", base, path);
    if (mkdirs(path_tmp, 0755)) {
        goto l_path_setup_error;
    }

    if (*destptr) {
        guard_free(*destptr);
    }

    (*destptr) = realpath(path_tmp, NULL);
    if (!*destptr) {
        goto l_path_setup_error;
    }

    guard_free(path_tmp);
    return 0;

    l_path_setup_error:
    guard_free(path_tmp);
    return -1;
}

int xml_pretty_print_in_place(const char *filename, const char *pretty_print_prog, const char *pretty_print_args) {
    int status = 0;
    char *tempfile = NULL;
    char *result = NULL;
    FILE *fp = NULL;
    FILE *tmpfp = NULL;
    char cmd[PATH_MAX];
    if (!find_program(pretty_print_prog)) {
        // Pretty printing is optional. 99% chance the XML data will
        // be passed to a report generator; not inspected by a human.
        return 0;
    }
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd) - 1, "%s %s %s", pretty_print_prog, pretty_print_args, filename);
    result = shell_output(cmd, &status);
    if (status || !result) {
        goto pretty_print_failed;
    }

    tempfile = xmkstemp(&tmpfp, "w+");
    if (!tmpfp || !tempfile) {
        goto pretty_print_failed;
    }

    fprintf(tmpfp, "%s", result);
    fflush(tmpfp);
    fclose(tmpfp);

    fp = fopen(filename, "w+");
    if (!fp) {
        goto pretty_print_failed;
    }

    if (copy2(tempfile, filename, CT_PERM)) {
        goto pretty_print_failed;
    }

    if (remove(tempfile)) {
        goto pretty_print_failed;
    }

    guard_free(tempfile);
    guard_free(result);
    return 0;

    pretty_print_failed:
        fprintf(SYSERROR);
        if (fp) {
            fclose(fp);
        }
        if (tmpfp) {
            fclose(tmpfp);
        }
        guard_free(tempfile);
        guard_free(result);
        return -1;
}

int fix_tox_conf(const char *filename, char **result) {
    struct INIFILE *toxini;
    FILE *fptemp;
    char *tempfile;
    const char *with_posargs = " \\\n    {posargs}\n";

    tempfile = xmkstemp(&fptemp, "w+");
    if (!tempfile) {
        return -1;
    }

    if (!*result) {
        *result = calloc(PATH_MAX, sizeof(*result));
        if (!*result) {
            return -1;
        }
    }

    toxini = ini_open(filename);
    if (!toxini) {
        if (fptemp) {
            guard_free(tempfile);
            fclose(fptemp);
        }
        guard_free(*result);
        return -1;
    }

    for (size_t i = 0; i < toxini->section_count; i++) {
        struct INISection *section = toxini->section[i];
        if (section) {
            if (startswith(section->key, "testenv")) {
                for (size_t k = 0; k < section->data_count; k++) {
                    struct INIData *data = section->data[k];
                    if (data) {
                        if (!strcmp(data->key, "commands") && (startswith(data->value, "pytest") && !strstr(data->value, "{posargs}"))) {
                            strip(data->value);
                            data->value = realloc(data->value, strlen(data->value) + strlen(with_posargs) + 1);
                            strcat(data->value, with_posargs);
                        }
                    }
                }
            }
        }
    }

    if (ini_write(toxini, &fptemp)) {
        guard_free(tempfile);
    }
    fclose(fptemp);

    *result = tempfile;
    ini_free(&toxini);
    return 0;
}

static size_t count_blanks(char *s) {
    size_t blank = 0;
    for (size_t i = 0; i < strlen(s); i++) {
        if (isblank(s[i])) {
            blank++;
        } else {
            break;
        }
    }
    return blank;
}

char *collapse_whitespace(char **s) {
    char *x = (*s);
    size_t len = strlen(x);
    for (size_t i = 0; i < len; i++) {
        size_t blank = count_blanks(&x[i]);
        if (blank > 1) {
            memmove(&x[i], &x[i] + blank, strlen(&x[i]));
        }
    }

    return *s;
}
