//
// Created by jhunk on 10/5/23.
//

#include "download.h"

size_t download_writer(void *fp, size_t size, size_t nmemb, void *stream) {
    size_t bytes = fwrite(fp, size, nmemb, (FILE *) stream);
    return bytes;
}

int download(char *url, const char *filename) {
    CURL *c;
    FILE *fp;

    curl_global_init(CURL_GLOBAL_ALL);
    c = curl_easy_init();
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, download_writer);
    fp = fopen(filename, "wb");
    if (!fp) {
        return 1;
    }
    //curl_easy_setopt(c, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, fp);
    curl_easy_perform(c);
    fclose(fp);

    curl_easy_cleanup(c);
    curl_global_cleanup();
    return 0;
}