/**
 * @file str.c
 */
#include <unistd.h>
#include "str.h"

int num_chars(const char *sptr, int ch) {
    int result = 0;
    for (int i = 0; sptr[i] != '\0'; i++) {
        if (sptr[i] == ch) {
            result++;
        }
    }
    return result;
}

int startswith(const char *sptr, const char *pattern) {
    if (!sptr || !pattern) {
        return 0;
    }
    for (size_t i = 0; i < strlen(pattern); i++) {
        if (sptr[i] != pattern[i]) {
            return 0;
        }
    }
    return 1;
}

int endswith(const char *sptr, const char *pattern) {
    if (!sptr || !pattern) {
        return 0;
    }
    ssize_t sptr_size = (ssize_t) strlen(sptr);
    ssize_t pattern_size = (ssize_t) strlen(pattern);

    if (sptr_size == pattern_size) {
       if (strcmp(sptr, pattern) == 0) {
           return 1; // yes
       }
       return 0; // no
    }

    ssize_t s = sptr_size - pattern_size;
    if (s < 0) {
        return 0;
    }

    for (size_t p = 0 ; s < sptr_size; s++, p++) {
        if (sptr[s] != pattern[p]) {
            // sptr does not end with pattern
            return 0;
        }
    }
    // sptr ends with pattern
    return 1;
}

void strchrdel(char *sptr, const char *chars) {
    if (sptr == NULL || chars == NULL) {
        return;
    }

    for (size_t i = 0; i < strlen(chars); i++) {
        char ch[2] = {0};
        strncpy(ch, &chars[i], 1);
        replace_text(sptr, ch, "", 0);
    }
}

char** split(char *_sptr, const char* delim, size_t max)
{
    if (_sptr == NULL || delim == NULL) {
        return NULL;
    }
    size_t split_alloc = 0;
    // Duplicate the input string and save a copy of the pointer to be freed later
    char *orig = _sptr;
    char *sptr = strdup(orig);

    if (!sptr) {
        return NULL;
    }

    // Determine how many delimiters are present
    for (size_t i = 0; i < strlen(delim); i++) {
        if (max && i > max) {
            break;
        }
        split_alloc += num_chars(sptr, delim[i]);
    }

    // Preallocate enough records based on the number of delimiters
    char **result = calloc(split_alloc + 2, sizeof(result[0]));
    if (!result) {
        guard_free(sptr);
        return NULL;
    }

    // No delimiter, but the string was not NULL, so return the original string
    if (split_alloc == 0) {
        result[0] = sptr;
        return result;
    }

    // Separate the string into individual parts and store them in the result array
    char *token = NULL;
    char *sptr_tmp = sptr;
    size_t pos = 0;
    size_t i;
    for (i = 0; (token = strsep(&sptr_tmp, delim)) != NULL; i++) {
        // When max is zero, record all tokens
        if (max > 0 && i == max) {
            // Maximum number of splits occurred.
            // Record position in string
            pos = token - sptr;
            break;
        }
        result[i] = calloc(STASIS_BUFSIZ, sizeof(char));
        if (!result[i]) {
            return NULL;
        }
        strcpy(result[i], token);
    }

    // pos is non-zero when maximum split is reached
    if (pos) {
        // append the remaining string contents to array
        result[i] = calloc(STASIS_BUFSIZ, sizeof(char));
        if (!result[i]) {
            return NULL;
        }
        strcpy(result[i], &orig[pos]);
    }

    guard_free(sptr);
    return result;
}

char *join(char **arr, const char *separator) {
    char *result = NULL;
    int records = 0;
    size_t total_bytes = 0;

    if (!arr || !separator) {
        return NULL;
    }

    for (int i = 0; arr[i] != NULL; i++) {
        total_bytes += strlen(arr[i]);
        records++;
    }
    total_bytes += (records * strlen(separator)) + 1;

    result = (char *)calloc(total_bytes, sizeof(char));
    for (int i = 0; i < records; i++) {
        strcat(result, arr[i]);
        if (i < (records - 1)) {
            strcat(result, separator);
        }
    }
    return result;
}

char *join_ex(char *separator, ...) {
    va_list ap;                 // Variadic argument list
    size_t separator_len = 0;   // Length of separator string
    size_t size = 0;            // Length of output string
    size_t argc = 0;            // Number of arguments ^ "..."
    char **argv = NULL;         // Arguments
    char *current = NULL;       // Current argument
    char *result = NULL;        // Output string

    if (separator == NULL) {
        return NULL;
    }

    // Initialize array
    argv = calloc(argc + 1, sizeof(char **));
    if (argv == NULL) {
        perror("join_ex calloc failed");
        return NULL;
    }

    // Get length of the separator
    separator_len = strlen(separator);

    // Process variadic arguments:
    // 1. Iterate over argument list `ap`
    // 2. Assign `current` with the value of argument in `ap`
    // 3. Extend the `argv` array by the latest argument count `argc`
    // 4. Sum the length of the argument and the `separator` passed to the function
    // 5. Append `current` string to `argv` array
    // 6. Update argument counter `argc`
    va_start(ap, separator);
    for(argc = 0; (current = va_arg(ap, char *)) != NULL; argc++) {
        char **tmp = realloc(argv, (argc + 1) * sizeof(char *));
        if (tmp == NULL) {
            perror("join_ex realloc failed");
            GENERIC_ARRAY_FREE(argv);
            return NULL;
        }
        argv = tmp;
        size += strlen(current) + separator_len;
        argv[argc] = strdup(current);
    }
    va_end(ap);

    // Generate output string
    result = calloc(size + 1, sizeof(char));
    for (size_t i = 0; i < argc; i++) {
        // Append argument to string
        strcat(result, argv[i]);

        // Do not append a trailing separator when we reach the last argument
        if (i < (argc - 1)) {
            strcat(result, separator);
        }
        guard_free(argv[i]);
    }
    guard_free(argv);

    return result;
}

char *substring_between(char *sptr, const char *delims) {
    char delim_open[255] = {0};
    char delim_close[255] = {0};
    if (sptr == NULL || delims == NULL) {
        return NULL;
    }

    // Ensure we have enough delimiters to continue
    size_t delim_count = strlen(delims);
    if (delim_count < 2 || delim_count % 2 || (delim_count > (sizeof(delim_open) - 1)) != 0) {
        return NULL;
    }
    size_t delim_take = delim_count / 2;

    // How else am I supposed to consume the first and last n chars of the string? Give me a break.
    // warning: ‘__builtin___strncpy_chk’ specified bound depends on the length of the source argument
    // ---
    //strncpy(delim_open, delims, delim_take);
    size_t i = 0;
    while (i < delim_take && i < sizeof(delim_open)) {
        delim_open[i] = delims[i];
        i++;
    }

    //strncpy(delim_close, &delims[delim_take], delim_take);
    i = 0;
    while (i < delim_take && i < sizeof(delim_close)) {
        delim_close[i] = delims[i + delim_take];
        i++;
    }

    // Create pointers to the delimiters
    char *start = strstr(sptr, delim_open);
    if (start == NULL || strlen(start) == 0) {
        return NULL;
    }

    char *end = strstr(start + 1, delim_close);
    if (end == NULL) {
        return NULL;
    }

    start += delim_count / 2;    // ignore leading delimiter

    // Get length of the substring
    size_t length = strlen(start) - strlen(end);
    if (!length) {
        return NULL;
    }

    // Return the contents of the substring
    return strndup(start, length);
}

/*
 * Comparison functions for `strsort`
 */
static int strsort_alpha_compare(const void *a, const void *b) {
    const char *aa = *(const char **)a;
    const char *bb = *(const char **)b;
    int result = strcmp(aa, bb);
    return result;
}

static int strsort_numeric_compare(const void *a, const void *b) {
    const char *aa = *(const char **)a;
    const char *bb = *(const char **)b;

    if (isdigit(*aa) && isdigit(*bb)) {
        long ia = strtol(aa, NULL, 10);
        long ib = strtol(bb, NULL, 10);

        if (ia == ib) {
            return 0;
        } else if (ia < ib) {
            return -1;
        } else if (ia > ib) {
            return 1;
        }
    }
    return 0;
}

static int strsort_asc_compare(const void *a, const void *b) {
    const char *aa = *(const char**)a;
    const char *bb = *(const char**)b;
    size_t len_a = strlen(aa);
    size_t len_b = strlen(bb);
    return len_a > len_b;
}

/*
 * Helper function for `strsortlen`
 */
static int strsort_dsc_compare(const void *a, const void *b) {
    const char *aa = *(const char**)a;
    const char *bb = *(const char**)b;
    size_t len_a = strlen(aa);
    size_t len_b = strlen(bb);
    return len_a < len_b;
}

void strsort(char **arr, unsigned int sort_mode) {
    if (arr == NULL) {
        return;
    }

    typedef int (*compar)(const void *, const void *);
    // Default mode is alphabetic sort
    compar fn = strsort_alpha_compare;

    if (sort_mode == STASIS_SORT_LEN_DESCENDING) {
        fn = strsort_dsc_compare;
    } else if (sort_mode == STASIS_SORT_LEN_ASCENDING) {
        fn = strsort_asc_compare;
    } else if (sort_mode == STASIS_SORT_ALPHA) {
        fn = strsort_alpha_compare; // ^ still selectable though ^
    } else if (sort_mode == STASIS_SORT_NUMERIC) {
        fn = strsort_numeric_compare;
    }

    size_t arr_size = 0;

    // Determine size of array (+ terminator)
    for (size_t i = 0; arr[i] != NULL; i++) {
        arr_size = i;
    }
    arr_size++;

    qsort(arr, arr_size, sizeof(char *), fn);
}


char *strstr_array(char **arr, const char *str) {
    if (arr == NULL || str == NULL) {
        return NULL;
    }

    for (int i = 0; arr[i] != NULL; i++) {
        if (strstr(arr[i], str) != NULL) {
            return arr[i];
        }
    }
    return NULL;
}


char **strdeldup(char **arr) {
    if (!arr) {
        return NULL;
    }

    size_t records;
    // Determine the length of the array
    for (records = 0; arr[records] != NULL; records++) {}

    // Allocate enough memory to store the original array contents
    // (It might not have duplicate values, for example)
    char **result = (char **)calloc(records + 1, sizeof(char *));
    if (!result) {
        return NULL;
    }

    int rec = 0;
    size_t i = 0;
    while(i < records) {
        // Search for value in results
        if (strstr_array(result, arr[i]) != NULL) {
            // value already exists in results so ignore it
            i++;
            continue;
        }

        // Store unique value
        result[rec] = strdup(arr[i]);
        if (!result[rec]) {
            for (size_t die = 0; result[die] != NULL; die++) {
                guard_free(result[die]);
            }
            guard_free(result);
            return NULL;
        }

        i++;
        rec++;
    }
    return result;
}

char *lstrip(char *sptr) {
    char *tmp = sptr;
    size_t bytes = 0;

    if (sptr == NULL) {
        return NULL;
    }

    while (strlen(tmp) > 1 && (isblank(*tmp) || isspace(*tmp))) {
        bytes++;
        tmp++;
    }
    if (tmp != sptr) {
        memmove(sptr, sptr + bytes, strlen(sptr) - bytes);
        memset((sptr + strlen(sptr)) - bytes, '\0', bytes);
    }
    return sptr;
}

char *strip(char *sptr) {
    if (sptr == NULL) {
        return NULL;
    }

    size_t len = strlen(sptr);
    if (len == 0) {
        return sptr;
    } else if (len == 1 && (isblank(*sptr) || isspace(*sptr))) {
        *sptr = '\0';
        return sptr;
    } for (size_t i = len; i != 0; --i) {
        if (sptr[i] == '\0') {
            continue;
        }
        if (isspace(sptr[i]) || isblank(sptr[i])) {
            sptr[i] = '\0';
        } else {
            break;
        }
    }
    return sptr;
}

int isempty(char *sptr) {
    if (sptr == NULL) {
        return -1;
    }

    char *tmp = sptr;
    while (*tmp) {
        if (!isblank(*tmp) && !isspace(*tmp) && !iscntrl(*tmp)) {
            return 0;
        }
        tmp++;
    }
    return 1;
}


int isquoted(char *sptr) {
    const char *quotes = "'\"";

    if (sptr == NULL) {
        return -1;
    }

    char *quote_open = strpbrk(sptr, quotes);
    if (!quote_open) {
        return 0;
    }
    char *quote_close = strpbrk(quote_open + 1, quotes);
    if (!quote_close) {
        return 0;
    }
    return 1;
}

int isrelational(char ch) {
    char symbols[] = "~!=<>";
    char *symbol = symbols;
    while (*symbol != '\0') {
        if (ch == *symbol) {
            return 1;
        }
        symbol++;
    }
    return 0;
}

void print_banner(const char *s, int len) {
    size_t s_len = strlen(s);
    if (!s_len) {
        return;
    }
    for (size_t i = 0; i < (len / s_len); i++) {
        for (size_t c = 0; c < s_len; c++) {
            putchar(s[c]);
        }
    }
    putchar('\n');
}

/**
 * Collapse whitespace in `s`. The string is modified in place.
 * @param s
 * @return pointer to `s`
 */
char *normalize_space(char *s) {
    size_t trim_pos;
    int add_whitespace = 0;
    char *result = s;
    char *tmp;

    if (s == NULL) {
        return NULL;
    }

    if ((tmp = calloc(strlen(s) + 1, sizeof(char))) == NULL) {
        perror("could not allocate memory for temporary string");
        return NULL;
    }
    char *tmp_orig = tmp;

    // count whitespace, if any
    for (trim_pos = 0; isblank(s[trim_pos]); trim_pos++) {}
    // trim whitespace from the left, if any
    memmove(s, &s[trim_pos], strlen(&s[trim_pos]));
    // cull bytes not part of the string after moving
    size_t len = strlen(s);
    s[len - trim_pos] = '\0';

    // Generate a new string with extra whitespace stripped out
    while (*s != '\0') {
        // Skip over any whitespace, but record that we encountered it
        if (isblank(*s)) {
            s++;
            add_whitespace = 1;
            continue;
        }
        // This gate avoids filling tmp with whitespace; we want to make our own
        if (add_whitespace) {
            *tmp = ' ';
            tmp++;
            add_whitespace = 0;
        }
        // Write character in s to tmp
        *tmp = *s;
        // Increment string pointers
        s++;
        tmp++;
    }

    // Rewrite the input string
    strcpy(result, tmp_orig);
    guard_free(tmp_orig);
    return result;
}

char **strdup_array(char **array) {
    char **result = NULL;
    size_t elems = 0;

    // Guard
    if (array == NULL) {
        return NULL;
    }

    // Count elements in `array`
    for (elems = 0; array[elems] != NULL; elems++) {}

    // Create new array
    result = calloc(elems + 1, sizeof(*result));
    for (size_t i = 0; i < elems; i++) {
        result[i] = strdup(array[i]);
    }

    return result;
}

int strcmp_array(const char **a, const char **b) {
    size_t a_len = 0;
    size_t b_len = 0;

    // This could lead to false-positives depending on what the caller plans to achieve
    if (a == NULL && b == NULL) {
        return 0;
    } else if (a == NULL) {
        return -1;
    } else if (b == NULL) {
        return 1;
    }

    // Get length of arrays
    for (a_len = 0; a[a_len] != NULL; a_len++) {}
    for (b_len = 0; b[b_len] != NULL; b_len++) {}

    // Check lengths are equal
    if (a_len < b_len) return (int)(b_len - a_len);
    else if (a_len > b_len) return (int)(a_len - b_len);

    // Compare strings in the arrays returning the total difference in bytes
    int result = 0;
    for (size_t ai = 0, bi = 0 ;a[ai] != NULL || b[bi] != NULL; ai++, bi++) {
        int status = 0;
        if ((status = strcmp(a[ai], b[bi]) != 0)) {
            result += status;
        }
    }
    return result;
}

int isdigit_s(const char *s) {
    if (!s || !strlen(s)) {
        return 0;   // nothing to do, fail
    }
    for (size_t i = 0; s[i] != '\0'; i++) {
        if (isdigit(s[i]) == 0) {
            return 0;   // non-digit found, fail
        }
    }
    return 1;   // all digits, succeed
}

char *tolower_s(char *s) {
    for (size_t i = 0; s[i] != '\0'; i++) {
        s[i] = (char)tolower(s[i]);
    }
    return s;
}

char *to_short_version(const char *s) {
    char *result = strdup(s);
    if (!result) {
        return NULL;
    }
    strchrdel(result, ".");
    return result;
}

void unindent(char *s) {
    char *pos = NULL;
    size_t leading_spaces;

    // Set position to beginning of string
    pos = s;

    while (pos != NULL) {
        const size_t len = strlen(s);
        for (leading_spaces = 0; isspace(pos[leading_spaces]); leading_spaces++) {}

        // For each new line strip an indent
        if (leading_spaces >= 4 && len >= 4) {
            leading_spaces = 4;  // remove first level of indentation
            memmove(pos, pos + leading_spaces, len - leading_spaces);
            pos[len - leading_spaces] = '\0';
        }

        pos = strchr(pos, '\n');
        if (pos && strlen(pos)) {
            pos++;
        }
    }
}