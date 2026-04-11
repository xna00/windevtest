#ifndef PRINT_JOB_H
#define PRINT_JOB_H

#include <windows.h>
#include "http_client.h"

typedef struct _PrintTaskInfo {
    char job_id[256];
    char task_id[256];
    char file_id[256];
    char filename[256];
} PrintTaskInfo;

int get_waiting_print_jobs(HttpClient *client, const char *computer_id, PrintTaskInfo **tasks, int *count);
int report_task_succeeded(HttpClient *client, const char *task_id);

#endif