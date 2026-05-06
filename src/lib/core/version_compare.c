#include "version_compare.h"

const struct {
    const char *key;
    int value;
} WEIGHT[] = {
    {.key = "post", 1000},
    {.key = "rc", -1000},
    {.key = "dev", -2000},
};

/**
 * Sum each part of a '.'-delimited version string
 * @param str version string
 * @return sum of each part
 * @return -1 on error
 */
int version_sum(const char *str) {
    char *end;

    if (!str || isempty((char *) str)) {
        return -1;
    }

    int result = 0;
    int epoch = 0;
    char *s = strdup(str);
    if (!s) {
        return -1;
    }
    char *ptr = s;
    end = ptr;

    // Parsing stops at the first non-alpha, non-'.' character
    // Digits are processed until the first invalid character
    // I'm torn whether this should be considered an error
    int i = 0;
    while (end != NULL) {
        int tmp_result = 0;

        tmp_result = (int) strtoul(ptr, &end, 10);

        // Circumvent a bug which allows a smaller version to be greater
        // than a larger version
        // Bug:
        //   1.0.3 == 1 + 0 + 3 = 4
        //   2.0.0 == 2 + 0 + 0 = 2
        // Correction:
        //   ((1 * EPOCH_MOD) + 1).0.3 = 104
        //   ((2 * EPOCH_MOD) + 2).0.0 = 202
        if (!i && tmp_result && *end != ':') {
            result += tmp_result * EPOCH_MOD;
            i++;
        }

        ptr = end;
        if (*ptr == '.' || *ptr == '-') {
            ptr++;
        }
        else if (!epoch && *ptr == ':') {
            epoch = 1;
            result += EPOCH_MOD;
            ptr++;
        }
        else if (isalpha(*ptr)) {
            int adjusted = 0;
            for (size_t w = 0; w < sizeof(WEIGHT) / sizeof(WEIGHT[0]); w++) {
                const int has_suffix = strncasecmp(ptr, WEIGHT[w].key, strlen(WEIGHT[w].key)) == 0;
                if (has_suffix) {
                    // skip the suffix
                    ptr += strlen(WEIGHT[w].key);
                    // adjust result based on suffix weight
                    result += WEIGHT[w].value;
                    adjusted = 1;
                    break;
                }
            }

            if (!adjusted) {
                result += *ptr - ('a' - 1);
                ptr++;
            }
        }
        else {
            end = NULL;
        }

        if (tmp_result) {
            result += tmp_result;
        }
    }

    free(s);
    return result;
}

/**
 * Convert version operator(s) to flags
 * @param str input string
 * @return operator flags
 */
int version_parse_operator(const char *str) {
    const char *valid = "><=!";

    const char *pos = str;
    int result = 0;

    if (isempty((char *) str)) {
        return -1;
    }
    while ((pos = strpbrk(pos, valid)) != NULL) {
        switch (*pos) {
            case '>':
                result |= GT;
                break;
            case '<':
                result |= LT;
                break;
            case '=':
                result |= EQ;
                break;
            case '!':
                result |= NOT;
                break;
            default:
                return -1;
        }
        pos++;
    }

    return result;
}

static int version_has_epoch(const char *str) {
    const char *result = strchr(str, ':');
    return result ? 1 : 0;
}

/**
 * Compare version strings based on flag(s)
 * @param flags verison operators
 * @param aa version1
 * @param bb version2
 * @return 1 flag operation is true
 * @return 0 flag operation is false
 */
int version_compare(const int flags, const char *aa, const char *bb) {
    if (!flags || flags < 0) {
        return -1;
    }

    int result_a = version_sum(aa);
    if (result_a < 0) {
        return -1;
    }

    int result_b = version_sum(bb);
    if (result_b < 0) {
        return -1;
    }

    if (version_has_epoch(aa) && !version_has_epoch(bb)) {
        result_a -= EPOCH_MOD;
    }
    if (!version_has_epoch(aa) && version_has_epoch(bb)) {
        result_b -= EPOCH_MOD;
    }

    int result = 0;
    if (flags & GT && flags & EQ) {
        result |= result_a >= result_b;
    } else if (flags & LT && flags & EQ) {
        result |= result_a <= result_b;
    } else if (flags & NOT && flags & EQ) {
        result |= result_a != result_b;
    } else if (flags & GT) {
        result |= result_a > result_b;
    } else if (flags & LT) {
        result |= result_a < result_b;
    } else if (flags & EQ) {
        result |= result_a == result_b;
    }

    return result;
}