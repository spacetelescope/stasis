/**
 * @file relocation.c
 */
#include "relocation.h"
#include "str.h"

void replace_text(char *original, const char *target, const char *replacement) {
    char buffer[OHMYCAL_BUFSIZ];
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

void file_replace_text(const char* filename, const char* target, const char* replacement) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "unable to open for reading: %s\n", filename);
        return;
    }

    char buffer[OHMYCAL_BUFSIZ];
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

/**
 * Replace all occurrences of `spattern` with `sreplacement` in `data`
 *
 * ~~~{.c}
 * char *str = (char *)calloc(100, sizeof(char));
 * strcpy(str, "This are a test.");
 * replace_line(str, "are", "is");
 * // str is: "This is a test."
 * free(str);
 * ~~~
 *
 * @param data string to modify
 * @param spattern string value to replace
 * @param sreplacement replacement string value
 * @return success=0, error=-1
 */
ssize_t replace_line(char *data, const char *spattern, const char *sreplacement) {
    if (data == NULL || spattern == NULL || sreplacement == NULL) {
        return -1;
    }

    if (strlen(spattern) == 0 || strlen(sreplacement) == 0) {
        return 0;
    }

    ssize_t count_replaced = 0;

    char *token = NULL;
    char buf[OHMYCAL_BUFSIZ];
    char *bufp = buf;
    char output[OHMYCAL_BUFSIZ];
    memset(output, 0, sizeof(output));
    strcpy(buf, data);
    for (size_t i = 0; (token = strsep(&bufp, "\n")) != NULL; i++) {
        char *match = strstr(token, spattern);
        if (match) {
            strncat(output, token, strlen(token) - strlen(match));
            strcat(output, sreplacement);
            strcat(output, "\n");
            count_replaced++;
        } else {
            strcat(output, token);
            strcat(output, "\n");
        }
    }

    strcpy(data, output);
    return count_replaced;
}

/**
 * Replace all occurrences of `oldstr` in file `path` with `newstr`
 * @param filename file to modify
 * @param oldstr string to replace
 * @param newstr replacement string
 * @return success=0, failure=-1, or value of `ferror()`
 */
int file_replace_line(char *filename, const char *spattern, const char *sreplacement) {
    char data[OHMYCAL_BUFSIZ];
    char tempfile[PATH_MAX];
    FILE *fp = NULL;
    if ((fp = fopen(filename, "r")) == NULL) {
        perror(filename);
        return -1;
    }

    sprintf(tempfile, "%s.replacement", filename);
    FILE *tfp = NULL;
    if ((tfp = fopen(tempfile, "w+")) == NULL) {
        fclose(fp);
        perror(tempfile);
        return -1;
    }

    // Zero the data buffer
    memset(data, '\0', OHMYCAL_BUFSIZ);
    while(fgets(data, OHMYCAL_BUFSIZ, fp) != NULL) {
        replace_line(data, spattern, sreplacement);
        fprintf(tfp, "%s", data);
        memset(data, 0, sizeof(data));
    }
    fclose(fp);
    fflush(tfp);
    rewind(tfp);

    // Truncate the original file
    if ((fp = fopen(filename, "w+")) == NULL) {
        perror(filename);
        return -1;
    }
    // Zero the data buffer once more
    memset(data, '\0', OHMYCAL_BUFSIZ);
    // Dump the contents of the temporary file into the original file
    while(fgets(data, OHMYCAL_BUFSIZ, tfp) != NULL) {
        fprintf(fp, "%s", data);
    }
    fclose(fp);
    fclose(tfp);

    // Remove temporary file
    unlink(tempfile);
    return 0;
}
