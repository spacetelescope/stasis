/**
 * @file relocation.c
 */
#include "relocation.h"
#include "str.h"

/**
 * Replace all occurrences of `target` with `replacement` in `original`
 *
 * ~~~{.c}
 * char *str = (char *)calloc(100, sizeof(char));
 * strcpy(str, "This are a test.");
 * replace_text(str, "are", "is");
 * // str is: "This is a test."
 * free(str);
 * ~~~
 *
 * @param original string to modify
 * @param target string value to replace
 * @param replacement string value
 */
void replace_text(char *original, const char *target, const char *replacement) {
    char buffer[OMC_BUFSIZ];
    char *tmp = original;

    memset(buffer, 0, sizeof(buffer));
    while (*tmp != '\0') {
        if (!strncmp(tmp, target, strlen(target))) {
            size_t replen;
            char *stop_at = strchr(tmp, '\n');
            if (stop_at) {
                replen = (stop_at - tmp);
            } else {
                replen = strlen(replacement);
            }
            strcat(buffer, replacement);
            strcat(buffer, "\n");
            tmp += replen;
        } else {
            strncat(buffer, tmp, 1);
        }
        tmp++;
    }
    strcpy(original, buffer);
}

/**
 * Replace `target` with `replacement` in `filename`
 *
 * ~~~{.c}
 * file_replace_text("/path/to/file.txt, "are", "is");
 * ~~~
 *
 * @param filename path to file
 * @param target string value to replace
 * @param replacement string
 */
void file_replace_text(const char* filename, const char* target, const char* replacement) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "unable to open for reading: %s\n", filename);
        return;
    }

    char buffer[OMC_BUFSIZ];
    char tempfilename[] = "tempfileXXXXXX";
    FILE *tfp = fopen(tempfilename, "w");

    if (tfp == NULL) {
        fprintf(stderr, "unable to open temporary fp for writing: %s\n", tempfilename);
        fclose(fp);
        return;
    }

    // Write modified strings to temporary file
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, target)) {
            replace_text(buffer, target, replacement);
        }
        fputs(buffer, tfp);
    }

    fclose(fp);
    fclose(tfp);

    // Replace original with modified copy
    remove(filename);
    rename(tempfilename, filename);
}