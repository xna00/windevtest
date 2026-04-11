#ifndef FILE_DOWNLOADER_H
#define FILE_DOWNLOADER_H

#include "http_client.h"

int download_file_to_local(HttpClient *client, const char *file_id, const char *filename, char *local_path, size_t path_size);
int download_and_print_file(HttpClient *client, const char *file_id);

#endif