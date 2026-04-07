#include "print_job.h"
#include "http_client.h"
#include "config.h"
#include <json-c/json.h>
#include <winspool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int print_file_to_default_printer(const char *file_data, size_t file_size) {
    char printer_name[256];
    DWORD buf_size = sizeof(printer_name);
    
    if (!GetDefaultPrinterA(printer_name, &buf_size)) {
        printf("Failed to get default printer\n");
        return -1;
    }
    
    printf("Using printer: %s\n", printer_name);
    
    HANDLE hPrinter;
    if (!OpenPrinterA(printer_name, &hPrinter, NULL)) {
        printf("Failed to open printer\n");
        return -1;
    }
    
    DOC_INFO_1 doc_info;
    memset(&doc_info, 0, sizeof(doc_info));
    doc_info.pDocName = "PrintJob";
    doc_info.pOutputFile = NULL;
    doc_info.pDatatype = "RAW";
    
    DWORD job_id = StartDocPrinterA(hPrinter, 1, (LPBYTE)&doc_info);
    if (job_id == 0) {
        printf("Failed to start document\n");
        ClosePrinter(hPrinter);
        return -1;
    }
    
    if (!StartPagePrinter(hPrinter)) {
        printf("Failed to start page\n");
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return -1;
    }
    
    DWORD written;
    if (!WritePrinter(hPrinter, (LPVOID)file_data, (DWORD)file_size, &written)) {
        printf("Failed to write to printer\n");
        EndPagePrinter(hPrinter);
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return -1;
    }
    
    EndPagePrinter(hPrinter);
    EndDocPrinter(hPrinter);
    ClosePrinter(hPrinter);
    
    printf("Print job submitted successfully (%lu bytes)\n", written);
    return 0;
}

int get_waiting_print_jobs(HttpClient *client, PrintTaskInfo **tasks, int *count) {
    char *response = NULL;
    long status_code = 0;
    
    const char *cookie = http_client_get_cookie(client);
    int ret = http_get_with_cookie(client, API_WAITING_PRINTJOBS, cookie, &response, &status_code);
    
    if (ret != 0 || status_code != 200 || !response) {
        printf("Failed to get waiting print jobs\n");
        return -1;
    }
    
    json_object *root = parse_json_response(response);
    if (!root) {
        free(response);
        return -1;
    }
    
    json_object *jobs_array;
    if (!json_object_object_get_ex(root, "printJobs", &jobs_array)) {
        json_object_put(root);
        free(response);
        return -1;
    }
    
    int array_len = json_object_array_length(jobs_array);
    if (array_len == 0) {
        json_object_put(root);
        free(response);
        *count = 0;
        return 0;
    }
    
    *tasks = (PrintTaskInfo *)malloc(sizeof(PrintTaskInfo) * array_len);
    *count = 0;
    
    for (int i = 0; i < array_len; i++) {
        json_object *job = json_object_array_get_idx(jobs_array, i);
        
        json_object *job_id_obj;
        json_object *print_tasks_obj;
        
        if (json_object_object_get_ex(job, "id", &job_id_obj) &&
            json_object_object_get_ex(job, "printTasks", &print_tasks_obj)) {
            
            const char *job_id = json_object_get_string(job_id_obj);
            int task_count = json_object_array_length(print_tasks_obj);
            
            for (int j = 0; j < task_count; j++) {
                json_object *task = json_object_array_get_idx(print_tasks_obj, j);
                
                json_object *task_id_obj, *file_id_obj;
                if (json_object_object_get_ex(task, "id", &task_id_obj) &&
                    json_object_object_get_ex(task, "fileId", &file_id_obj)) {
                    
                    strncpy((*tasks)[*count].job_id, job_id, sizeof((*tasks)[*count].job_id) - 1);
                    strncpy((*tasks)[*count].task_id, json_object_get_string(task_id_obj), sizeof((*tasks)[*count].task_id) - 1);
                    strncpy((*tasks)[*count].file_id, json_object_get_string(file_id_obj), sizeof((*tasks)[*count].file_id) - 1);
                    (*count)++;
                }
            }
        }
    }
    
    json_object_put(root);
    free(response);
    return 0;
}

int download_and_print_file(HttpClient *client, const char *file_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s%s", API_GET_FILE, file_id);
    
    char *response = NULL;
    long status_code = 0;
    
    const char *cookie = http_client_get_cookie(client);
    int ret = http_get_with_cookie(client, url, cookie, &response, &status_code);
    
    if (ret != 0 || status_code != 200 || !response) {
        printf("Failed to download file %s\n", file_id);
        return -1;
    }
    
    size_t file_size = strlen(response);
    int print_result = print_file_to_default_printer(response, file_size);
    
    free(response);
    return print_result;
}

int report_task_succeeded(HttpClient *client, const char *task_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s?taskId=%s", API_TASK_SUCCEED, task_id);
    
    char *response = NULL;
    long status_code = 0;
    
    const char *cookie = http_client_get_cookie(client);
    int ret = http_get_with_cookie(client, url, cookie, &response, &status_code);
    
    if (response) free(response);
    
    return ret;
}