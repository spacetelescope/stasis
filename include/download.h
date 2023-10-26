//
// Created by jhunk on 10/5/23.
//

#ifndef OHMYCAL_DOWNLOAD_H
#define OHMYCAL_DOWNLOAD_H

#include <curl/curl.h>

size_t download_writer(void *fp, size_t size, size_t nmemb, void *stream);
int download(char *url, const char *filename);

#endif //OHMYCAL_DOWNLOAD_H
