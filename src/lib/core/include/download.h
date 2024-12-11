//! @file download.h
#ifndef STASIS_DOWNLOAD_H
#define STASIS_DOWNLOAD_H

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

size_t download_writer(void *fp, size_t size, size_t nmemb, void *stream);
long download(char *url, const char *filename, char **errmsg);

#endif //STASIS_DOWNLOAD_H
