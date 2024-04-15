//
// Created by jhunk on 10/5/23.
//

#include <string.h>
#include "download.h"

size_t download_writer(void *fp, size_t size, size_t nmemb, void *stream) {
    size_t bytes = fwrite(fp, size, nmemb, (FILE *) stream);
    return bytes;
}

long download(char *url, const char *filename, char **errmsg) {
    extern char *VERSION;
    CURL *c;
    CURLcode curl_code;
    long http_code = -1;
    FILE *fp;
    char user_agent[20];
    sprintf(user_agent, "omc/%s", VERSION);

    curl_global_init(CURL_GLOBAL_ALL);
    c = curl_easy_init();
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, download_writer);
    fp = fopen(filename, "wb");
    if (!fp) {
        return -1;
    }
    curl_easy_setopt(c, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, user_agent);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, fp);
    curl_code = curl_easy_perform(c);
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
    fclose(fp);
    curl_easy_cleanup(c);
    curl_global_cleanup();
    return http_code;
}