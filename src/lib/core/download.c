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
    sprintf(user_agent, "stasis/%s", VERSION);
    long timeout = 30L;
    char *timeout_str = getenv("STASIS_DOWNLOAD_TIMEOUT");

    curl_global_init(CURL_GLOBAL_ALL);
    CURL *c = curl_easy_init();
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

    if (timeout_str) {
        timeout = strtol(timeout_str, NULL, 10);
    }
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, timeout);

    SYSDEBUG("curl_easy_perform(): \n\turl=%s\n\tfilename=%s\n\tuser agent=%s\n\ttimeout=%zu", url, filename, user_agent, timeout);
    CURLcode curl_code = curl_easy_perform(c);
    SYSDEBUG("curl status code: %d", curl_code);
    if (curl_code != CURLE_OK) {
        if (errmsg) {
            strcpy(*errmsg, curl_easy_strerror(curl_code));
        } else {
            fprintf(stderr, "\nCURL ERROR: %s\n", curl_easy_strerror(curl_code));
        }
        goto failed;
    }
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);
    failed:
    SYSDEBUG("HTTP code: %li", http_code);
    fclose(fp);
    curl_easy_cleanup(c);
    curl_global_cleanup();
    return http_code;
}