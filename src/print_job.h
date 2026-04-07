#ifndef PRINT_JOB_H
#define PRINT_JOB_H

#include <windows.h>
#include "http_client.h"

typedef struct _PrintTaskInfo {
    char job_id[256];
    char task_id[256];
    char file_id[256];
} PrintTaskInfo;

int print_file_to_default_printer(const char *file_data, size_t file_size);
int get_waiting_print_jobs(HttpClient *client, PrintTaskInfo **tasks, int *count);
int download_and_print_file(HttpClient *client, const char *file_id);
int report_task_succeeded(HttpClient *client, const char *task_id);

#endif