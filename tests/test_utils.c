#include "testing.h"

char cwd_start[PATH_MAX];
char cwd_workspace[PATH_MAX];
extern char *dirstack[];
extern const ssize_t dirstack_max;
extern ssize_t dirstack_len;

void test_listdir() {
    const char *dirs[] = {
            "test_listdir",
            "test_listdir/1",
            "test_listdir/2",
            "test_listdir/3",
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(*dirs); i++) {
        mkdir(dirs[i], 0755);
    }
    struct StrList *listing;
    listing = listdir(dirs[0]);
    OMC_ASSERT(listing != NULL, "listdir failed");
    OMC_ASSERT(listing && (strlist_count(listing) == (sizeof(dirs) / sizeof(*dirs)) - 1), "should have 3");
    guard_strlist_free(&listing);
}

void test_redact_sensitive() {
    const char *data[] = {
            "100 dollars!",
            "bananas apples pears",
            "have a safe trip",
    };
    const char *to_redact[] = {
            "dollars",
            "bananas",
            "have a safe trip",
            NULL,
    };
    const char *expected[] = {
            "100 ***REDACTED***!",
            "***REDACTED*** apples pears",
            "***REDACTED***",
    };

    for (size_t i = 0; i < sizeof(data) / sizeof(*data); i++) {
        char *input = strdup(data[i]);
        char output[100] = {0};
        redact_sensitive(to_redact, input, output, sizeof(output) - 1);
        OMC_ASSERT(strcmp(output, expected[i]) == 0, "incorrect redaction");
    }
}

void test_fix_tox_conf() {
    const char *filename = "tox.ini";
    const char *data = "[testenv]\n"
                       "key_before = 1\n"
                       "commands = pytest -sx tests\n"
                       "key_after = 2\n";
    const char *expected = "{posargs}\n";
    char *result = NULL;
    FILE *fp;

    remove(filename);
    fp = fopen(filename, "w");
    if (fp) {
        fprintf(fp, "%s", data);
        fclose(fp);
        OMC_ASSERT(fix_tox_conf(filename, &result) == 0, "fix_tox_conf failed");
    } else {
        OMC_ASSERT(false, "writing mock tox.ini failed");
    }

    char **lines = file_readlines(result, 0, 0, NULL);
    OMC_ASSERT(strstr_array(lines, expected) != NULL, "{posargs} not found in result");

    remove(result);
    guard_free(result);
}

void test_xml_pretty_print_in_place() {
    FILE *fp;
    const char *filename = "ugly.xml";
    const char *data = "<things><abc>123</abc><abc>321</abc></things>";
    const char *expected = "<?xml version=\"1.0\"?>\n"
                           "<things>\n"
                           "  <abc>123</abc>\n"
                           "  <abc>321</abc>\n"
                           "</things>\n";

    remove(filename);
    fp = fopen(filename, "w");
    if (fp) {
        fprintf(fp, "%s", data);
        fclose(fp);
        OMC_ASSERT(xml_pretty_print_in_place(
                filename,
                OMC_XML_PRETTY_PRINT_PROG,
                OMC_XML_PRETTY_PRINT_ARGS) == 0,
                   "xml pretty print failed (xmllint not installed?)");
    } else {
        OMC_ASSERT(false, "failed to create input file");
    }

    fp = fopen(filename, "r");
    char buf[BUFSIZ] = {0};
    if (fread(buf, sizeof(*buf), sizeof(buf) - 1, fp) < 1) {
        OMC_ASSERT(false, "failed to consume formatted xml file contents");
    }
    OMC_ASSERT(strcmp(expected, buf) == 0, "xml file was not reformatted");
}

void test_path_store() {
    char *dest = NULL;
    chdir(cwd_workspace);
    int result = path_store(&dest, PATH_MAX, cwd_workspace, "test_path_store");
    OMC_ASSERT(result == 0, "path_store encountered an error");
    OMC_ASSERT(dest != NULL, "dest should not be NULL");
    OMC_ASSERT(dest && (access(dest, F_OK) == 0), "destination path was not created");
    rmtree(dest);
}

void test_isempty_dir() {
    const char *dname = "test_isempty_dir";
    rmtree(dname);
    mkdir(dname, 0755);
    OMC_ASSERT(isempty_dir(dname) > 0, "empty directory went undetected");

    char path[PATH_MAX];
    sprintf(path, "%s/file.txt", dname);
    touch(path);

    OMC_ASSERT(isempty_dir(dname) == 0, "populated directory went undetected");
}

void test_xmkstemp() {
    FILE *tempfp = NULL;
    char *tempfile;
    const char *data = "Hello, world!\n";

    tempfile = xmkstemp(&tempfp, "w");
    OMC_ASSERT(tempfile != NULL, "failed to create temporary file");
    fprintf(tempfp, "%s", data);
    fclose(tempfp);

    char buf[100] = {0};
    tempfp = fopen(tempfile, "r");
    fgets(buf, sizeof(buf) - 1, tempfp);
    fclose(tempfp);

    OMC_ASSERT(strcmp(buf, data) == 0, "data written to temp file is incorrect");
    remove(tempfile);
    free(tempfile);
}

void test_msg() {
    int flags_indent[] = {
        0,
        OMC_MSG_L1,
        OMC_MSG_L2,
        OMC_MSG_L3,
    };
    int flags_info[] = {
        //OMC_MSG_NOP,
        OMC_MSG_SUCCESS,
        OMC_MSG_WARN,
        OMC_MSG_ERROR,
        //OMC_MSG_RESTRICT,
    };
    for (size_t i = 0; i < sizeof(flags_indent) / sizeof(*flags_indent); i++) {
        for (size_t x = 0; x < sizeof(flags_info) / sizeof(*flags_info); x++) {
            msg(flags_info[x] | flags_indent[i], "Indent level %zu...Message %zu\n", i, x);
        }
    }
}

void test_git_clone_and_describe() {
    struct Process proc;
    memset(&proc, 0, sizeof(proc));
    const char *local_workspace = "test_git_clone";
    const char *repo = "localrepo";
    const char *repo_git = "localrepo.git";
    char *cwd = getcwd(NULL, PATH_MAX);

    // remove the bare repo, and local clone
    rmtree(repo);
    rmtree(repo_git);

    mkdir(local_workspace, 0755);
    chdir(local_workspace);

    // initialize a bare repo so we can clone it
    char init_cmd[PATH_MAX];
    sprintf(init_cmd, "git init --bare %s", repo_git);
    system(init_cmd);

    // clone the bare repo
    OMC_ASSERT(git_clone(&proc, repo_git, repo, NULL) == 0,
               "a local clone of bare repository should have succeeded");
    chdir(repo);

    // interact with it
    touch("README.md");
    system("git add README.md");
    system("git commit --no-gpg-sign -m Test");
    system("git push -u origin");
    system("git --no-pager log");

    // test git_describe is functional
    char *taginfo_none = git_describe(".");
    OMC_ASSERT(taginfo_none != NULL, "should be a git hash, not NULL");

    system("git tag -a 1.0.0 -m Mock");
    system("git push --tags origin");
    char *taginfo = git_describe(".");
    OMC_ASSERT(taginfo != NULL, "should be 1.0.0, not NULL");
    OMC_ASSERT(strcmp(taginfo, "1.0.0") == 0, "just-created tag was not described correctly");
    chdir("..");

    char *taginfo_outer = git_describe(repo);
    OMC_ASSERT(taginfo_outer != NULL, "should be 1.0.0, not NULL");
    OMC_ASSERT(strcmp(taginfo_outer, "1.0.0") == 0, "just-created tag was not described correctly (out-of-dir invocation)");

    char *taginfo_bad = git_describe("abc1234_not_here_or_there");
    OMC_ASSERT(taginfo_bad == NULL, "a repository that shouldn't exist... exists and has a tag.");
    chdir(cwd);
}

void test_touch() {
    OMC_ASSERT(touch("touchedfile.txt") == 0, "touch failed");
    OMC_ASSERT(access("touchedfile.txt", F_OK) == 0, "touched file does not exist");
    remove("touchedfile.txt");
}

void test_find_program() {
    OMC_ASSERT(find_program("willnotexist123") == NULL, "did not return NULL");
    OMC_ASSERT(find_program("find") != NULL, "program not available (OS dependent)");
}

static int file_readlines_callback_modify(size_t size, char **line) {
    char *data = (*line);
    for (size_t i = 0; i < strlen(data); i++) {
        if (isalnum(data[i])) {
            data[i] = 'x';
        }
    }
    return 0;
}

static int file_readlines_callback_get_specific_line(size_t size, char **line) {
    char *data = (*line);
    for (size_t i = 0; i < strlen(data); i++) {
        if (!strcmp(data, "see?\n")) {
            return 0;
        }
    }
    return 1;
}


void test_file_readlines() {
    const char *filename = "file_readlines.txt";
    const char *data = "I am\na file\nwith multiple lines\nsee?\n";
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror(filename);
        return;
    }
    if (fwrite(data, sizeof(*data), strlen(data), fp) != strlen(data)) {
        perror("short write");
        return;
    }
    fclose(fp);

    char **result = NULL;
    result = file_readlines(filename, 0, 0, NULL);
    int i;
    for (i = 0; result[i] != NULL; i++);
    OMC_ASSERT(num_chars(data, '\n') == i, "incorrect number of lines in data");
    OMC_ASSERT(strcmp(result[3], "see?\n") == 0, "last line in data is incorrect'");
    GENERIC_ARRAY_FREE(result);

    result = file_readlines(filename, 0, 0, file_readlines_callback_modify);
    OMC_ASSERT(strcmp(result[3], "xxx?\n") == 0, "last line should be: 'xxx?\\n'");
    GENERIC_ARRAY_FREE(result);

    result = file_readlines(filename, 0, 0, file_readlines_callback_get_specific_line);
    OMC_ASSERT(strcmp(result[0], "see?\n") == 0, "the first record of the result is not the last line of the file 'see?\\n'");
    GENERIC_ARRAY_FREE(result);
    remove(filename);
}

void test_path_dirname() {
    const char *data[] = {
            "a/b/c", "a/b",
            "This/is/a/test", "This/is/a",
    };
    for (size_t i = 0; i < sizeof(data) / sizeof(*data); i += 2) {
        const char *input = data[i];
        const char *expected = data[i + 1];
        char tmp[PATH_MAX] = {0};
        strcpy(tmp, input);

        char *result = path_dirname(tmp);
        OMC_ASSERT(strcmp(expected, result) == 0, NULL);
    }
}

void test_path_basename() {
    const char *data[] = {
        "a/b/c", "c",
        "This/is/a/test", "test",
    };
    for (size_t i = 0; i < sizeof(data) / sizeof(*data); i += 2) {
        const char *input = data[i];
        const char *expected = data[i + 1];
        char *result = path_basename(input);
        OMC_ASSERT(strcmp(expected, result) == 0, NULL);
    }
}

void test_expandpath() {
    char *home;

    const char *homes[] = {
            "HOME",
            "USERPROFILE",
    };
    for (size_t i = 0; i < sizeof(homes) / sizeof(*homes); i++) {
        home = getenv(homes[i]);
        if (home) {
            break;
        }
    }

    char path[PATH_MAX] = {0};
    strcat(path, "~");
    strcat(path, DIR_SEP);

    char *expanded = expandpath(path);
    OMC_ASSERT(startswith(expanded, home) > 0, expanded);
    OMC_ASSERT(endswith(expanded, DIR_SEP) > 0, "the input string ends with a directory separator, the result did not");
    free(expanded);
}

void test_rmtree() {
    const char *root = "rmtree_dir";
    const char *tree[] = {
            "abc",
            "123",
            "cba",
            "321"
    };
    chdir(cwd_workspace);

    mkdir(root, 0755);
    for (size_t i = 0; i < sizeof(tree) / sizeof(*tree); i++) {
        char path[PATH_MAX];
        char mockfile[PATH_MAX + 10];
        sprintf(path, "%s/%s", root, tree[i]);
        sprintf(mockfile, "%s/%zu.txt", path, i);
        if (mkdir(path, 0755)) {
            perror(path);
            OMC_ASSERT(false, NULL);
        }
        touch(mockfile);
    }
    OMC_ASSERT(rmtree(root) == 0, "rmtree should have been able to remove the directory");
    OMC_ASSERT(access(root, F_OK) < 0, "the directory is still present");
}

void test_dirstack() {
    const char *data[] = {
        "testdir",
        "1",
        "2",
        "3",
    };

    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    for (size_t i = 0; i < sizeof(data) / sizeof(*data); i++) {
        mkdir(data[i], 0755);
        pushd(data[i]);
        getcwd(cwd, PATH_MAX);
    }
    OMC_ASSERT(dirstack_len == sizeof(data) / sizeof(*data), NULL);
    OMC_ASSERT(strcmp(dirstack[0], cwd_workspace) == 0, NULL);

    for (int i = 1; i < dirstack_len; i++) {
        OMC_ASSERT(endswith(dirstack[i], data[i - 1]), NULL);
    }

    for (size_t i = 0, x = dirstack_len - 1; x != 0 && i < sizeof(data) / sizeof(*data); i++, x--) {
        char *expected = strdup(dirstack[x]);
        popd();
        getcwd(cwd, PATH_MAX);
        OMC_ASSERT(strcmp(cwd, expected) == 0, NULL);
        free(expected);
    }
}

void test_pushd_popd() {
    const char *dname = "testdir";
    chdir(cwd_workspace);
    rmtree(dname);

    OMC_ASSERT(mkdir(dname, 0755) == 0, "directory should not exist yet");
    OMC_ASSERT(pushd(dname) == 0, "failed to enter directory");
    char *cwd = getcwd(NULL, PATH_MAX);

    // we should be inside the test directory, not the starting directory
    OMC_ASSERT(strcmp(cwd_workspace, cwd) != 0, "");
    OMC_ASSERT(popd() == 0, "return from directory failed");

    char *cwd_after_popd = getcwd(NULL, PATH_MAX);
    OMC_ASSERT(strcmp(cwd_workspace, cwd_after_popd) == 0, "should be the path where we started");
}

void test_pushd_popd_suggested_workflow() {
    const char *dname = "testdir";
    chdir(cwd_workspace);
    rmtree(dname);

    remove(dname);
    if (!mkdir(dname, 0755)) {
        if (!pushd(dname)) {
            char *cwd = getcwd(NULL, PATH_MAX);
            OMC_ASSERT(strcmp(cwd_workspace, cwd) != 0, NULL);
            // return from dir
            popd();
            free(cwd);
        }
        // cwd should be our starting directory
        char *cwd = getcwd(NULL, PATH_MAX);
        OMC_ASSERT(strcmp(cwd_workspace, cwd) == 0, NULL);
    } else {
        OMC_ASSERT(false, "mkdir function failed");
    }
}


int main(int argc, char *argv[]) {
    OMC_TEST_BEGIN_MAIN();
    OMC_TEST_FUNC *tests[] = {
            test_listdir,
            test_redact_sensitive,
            test_fix_tox_conf,
            test_xml_pretty_print_in_place,
            test_path_store,
            test_isempty_dir,
            test_xmkstemp,
            test_msg,
            test_git_clone_and_describe,
            test_touch,
            test_find_program,
            test_file_readlines,
            test_path_dirname,
            test_path_basename,
            test_expandpath,
            test_rmtree,
            test_dirstack,
            test_pushd_popd,
            test_pushd_popd_suggested_workflow,
    };
    const char *ws = "workspace";
    getcwd(cwd_start, sizeof(cwd_start) - 1);
    mkdir(ws, 0755);
    chdir(ws);
    getcwd(cwd_workspace, sizeof(cwd_workspace) - 1);

    OMC_TEST_RUN(tests);

    chdir(cwd_start);
    if (rmtree(cwd_workspace)) {
        perror(cwd_workspace);
    }
    OMC_TEST_END_MAIN();
}