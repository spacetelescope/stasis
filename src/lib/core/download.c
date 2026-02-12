//
// Created by jhunk on 10/5/23.
//

#include "download.h"
#include "core.h"

size_t download_writer(void *fp, size_t size, size_t nmemb, void *stream) {
    size_t bytes = fwrite(fp, size, nmemb, (FILE *) stream);
    return bytes;
}

long download(char *url, const char *filename, char **errmsg) {
    long http_code = -1;
    char user_agent[20];
    snprintf(user_agent, sizeof(user_agent), "stasis/%s", VERSION);

    long timeout = 30L;
    const char *timeout_str = getenv("STASIS_DOWNLOAD_TIMEOUT");
    if (timeout_str) {
        timeout = strtol(timeout_str, NULL, 10);
        if (timeout <= 0L) {
            timeout = 1L;
        }
    }

    ssize_t max_retries = 5;
    const char *max_retries_str = getenv("STASIS_DOWNLOAD_RETRY_MAX");
    if (max_retries_str) {
        max_retries = strtol(max_retries_str, NULL, 10);
        if (max_retries <= 0) {
            max_retries = 1;
        }
    }

    ssize_t max_retry_seconds = 3;
    const char *max_retry_seconds_str = getenv("STASIS_DOWNLOAD_RETRY_SECONDS");
    if (max_retry_seconds_str) {
        max_retry_seconds = strtol(max_retry_seconds_str, NULL, 10);
        if (max_retry_seconds < 0) {
            max_retry_seconds = 0;
        }
    }


    curl_global_init(CURL_GLOBAL_ALL);
    CURL *c = curl_easy_init();
    for (ssize_t retry = 0; retry < max_retries; retry++) {
        if (retry) {
            fprintf(stderr, "[RETRY %zu/%zu] %s: %s\n", retry + 1, max_retries, *errmsg, url);
        }
        curl_easy_setopt(c, CURLOPT_URL, url);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, download_writer);
        FILE *fp = fopen(filename, "wb");
        if (!fp) {
            return -1;
        }

        curl_easy_setopt(c, CURLOPT_VERBOSE, 0L);
        curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(c, CURLOPT_USERAGENT, user_agent);
        curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, timeout);

        SYSDEBUG("curl_easy_perform(): \n\turl=%s\n\tfilename=%s\n\tuser agent=%s\n\ttimeout=%zu", url, filename, user_agent, timeout);
        CURLcode curl_code = curl_easy_perform(c);
        SYSDEBUG("curl status code: %d", curl_code);

        if (curl_code != CURLE_OK) {
            const size_t errmsg_maxlen = 256;
            if (!*errmsg) {
                *errmsg = calloc(errmsg_maxlen, sizeof(char));
            }
            snprintf(*errmsg, errmsg_maxlen, "%s", curl_easy_strerror(curl_code));
            curl_easy_reset(c);
            fclose(fp);
            sleep(max_retry_seconds);
            continue;
        }

        // Data written. Clean up.
        fclose(fp);

        if (CURLE_OK && *errmsg) {
            // Retry loop succeeded, no error
            *errmsg[0] = '\0';
        }

        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);
        SYSDEBUG("HTTP code: %li", http_code);

        break;
    }
    curl_easy_cleanup(c);
    curl_global_cleanup();
    return http_code;
}