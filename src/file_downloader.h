#ifndef FILE_DOWNLOADER_H
#define FILE_DOWNLOADER_H

struct HttpClient;

int download_file_to_local(struct HttpClient *client, const char *file_id, const char *filename, char *local_path, size_t path_size);
int download_and_print_file(struct HttpClient *client, const char *file_id);

#endif