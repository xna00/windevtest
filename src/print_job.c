/*
 * 打印任务实现
 * 
 * 功能说明:
 * - 使用Windows打印API (Winspool) 打印文件
 * - 调用API获取和处理打印任务
 */

#include "print_job.h"
#include "http_client.h"
#include "config.h"
#include <json-c/json.h>
#include <winspool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * 创建下载目录（如果不存在）
 */
static int ensure_download_folder(void) {
    struct stat st = {0};
    if (stat(DOWNLOAD_FOLDER, &st) == -1) {
        return _mkdir(DOWNLOAD_FOLDER);
    }
    return 0;
}

/*
 * 打印文件到默认打印机
 * 使用Windows Winspool API进行RAW模式打印
 * @param file_path 文件路径
 * @return 0成功，-1失败
 */
int print_file_to_default_printer(const char *file_path) {
    wchar_t printer_name[256];
    DWORD buf_size = sizeof(printer_name);
    
    /* 获取系统默认打印机名称 */
    if (!GetDefaultPrinterW(printer_name, &buf_size)) {
        printf("Failed to get default printer\n");
        return -1;
    }
    
    /* 打开打印机 */
    HANDLE hPrinter;
    if (!OpenPrinterW(printer_name, &hPrinter, NULL)) {
        printf("Failed to open printer\n");
        return -1;
    }
    
    /* 设置文档信息 */
    DOC_INFO_1W doc_info;
    memset(&doc_info, 0, sizeof(doc_info));
    doc_info.pDocName = L"PrintJob";
    doc_info.pOutputFile = NULL;
    doc_info.pDatatype = L"RAW";
    
    /* 开始打印文档 */
    DWORD job_id = StartDocPrinterW(hPrinter, 1, (LPBYTE)&doc_info);
    if (job_id == 0) {
        printf("Failed to start document\n");
        ClosePrinter(hPrinter);
        return -1;
    }
    
    /* 开始一页 */
    if (!StartPagePrinter(hPrinter)) {
        printf("Failed to start page\n");
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return -1;
    }
    
    /* 读取文件内容并打印 */
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        printf("Failed to open file: %s\n", file_path);
        EndPagePrinter(hPrinter);
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return -1;
    }
    
    /* 获取文件大小 */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* 分配缓冲区 */
    char *buffer = (char *)malloc(file_size);
    if (!buffer) {
        printf("Failed to allocate memory\n");
        fclose(fp);
        EndPagePrinter(hPrinter);
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return -1;
    }
    
    /* 读取文件 */
    size_t read_size = fread(buffer, 1, file_size, fp);
    fclose(fp);
    
    if (read_size != (size_t)file_size) {
        printf("Failed to read file completely\n");
        free(buffer);
        EndPagePrinter(hPrinter);
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return -1;
    }
    
    /* 发送打印数据 */
    DWORD written;
    if (!WritePrinter(hPrinter, (LPVOID)buffer, (DWORD)file_size, &written)) {
        printf("Failed to write to printer\n");
        free(buffer);
        EndPagePrinter(hPrinter);
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return -1;
    }
    
    free(buffer);
    
    /* 结束打印 */
    EndPagePrinter(hPrinter);
    EndDocPrinter(hPrinter);
    ClosePrinter(hPrinter);
    
    printf("Print job submitted successfully (%ld bytes)\n", file_size);
    return 0;
}

/*
 * 获取等待中的打印任务列表
 * 从服务器API获取待打印任务
 * 使用POST请求，body为"[]"
 */
int get_waiting_print_jobs(HttpClient *client, PrintTaskInfo **tasks, int *count) {
    char *response = NULL;
    long status_code = 0;
    
    /* 发送带Cookie的POST请求，body为"[]" */
    const char *cookie = http_client_get_cookie(client);
    int ret = http_post_with_client_cookie(client, API_WAITING_PRINTJOBS, "[]", &response, &status_code);
    
    if (ret != 0 || status_code != 200 || !response) {
        printf("Failed to get waiting print jobs\n");
        return -1;
    }
    
    /* 解析JSON响应 */
    json_object *root = parse_json_response(response);
    if (!root) {
        printf("Failed to parse JSON response\n");
        free(response);
        return -1;
    }
    
    /* 获取printJobs数组 - 响应直接是数组，不是对象 */
    json_object *jobs_array = NULL;
    json_type root_type = json_object_get_type(root);
    
    if (root_type == json_type_array) {
        /* 响应直接是数组 */
        jobs_array = root;
    } else {
        /* 响应是对象，尝试获取printJobs字段 */
        if (!json_object_object_get_ex(root, "printJobs", &jobs_array)) {
            printf("No printJobs in response\n");
            json_object_put(root);
            free(response);
            *count = 0;
            return 0;
        }
    }
    
    int array_len = json_object_array_length(jobs_array);
    if (array_len == 0) {
        printf("No print jobs found\n");
        json_object_put(root);
        free(response);
        *count = 0;
        return 0;
    }
    
    /* 计算最大可能的task数量 */
    int max_tasks = 0;
    for (int i = 0; i < array_len; i++) {
        json_object *job = json_object_array_get_idx(jobs_array, i);
        json_object *print_tasks_obj;
        if (json_object_object_get_ex(job, "printTasks", &print_tasks_obj)) {
            max_tasks += json_object_array_length(print_tasks_obj);
        }
    }
    
    /* 分配内存存储任务 */
    *tasks = (PrintTaskInfo *)malloc(sizeof(PrintTaskInfo) * max_tasks);
    *count = 0;
    
    /* 解析每个任务 */
    for (int i = 0; i < array_len; i++) {
        json_object *job = json_object_array_get_idx(jobs_array, i);
        
        json_object *job_id_obj;
        json_object *print_tasks_obj;
        
                /* 提取job id和printTasks数组 */
        if (json_object_object_get_ex(job, "id", &job_id_obj) &&
            json_object_object_get_ex(job, "printTasks", &print_tasks_obj)) {
            
            /* job id是整数，需要转换为字符串 */
            int job_id_int = json_object_get_int(job_id_obj);
            char job_id[32];
            snprintf(job_id, sizeof(job_id), "%d", job_id_int);
            
            int task_count = json_object_array_length(print_tasks_obj);
            
            printf("Job %s has %d task(s)\n", job_id, task_count);
            
            /* 遍历printTasks数组 */
            for (int j = 0; j < task_count; j++) {
                json_object *task = json_object_array_get_idx(print_tasks_obj, j);
                
                json_object *task_id_obj, *file_id_obj, *filename_obj;
                /* 提取task id、fileId和filename */
                if (json_object_object_get_ex(task, "id", &task_id_obj) &&
                    json_object_object_get_ex(task, "fileId", &file_id_obj)) {
                    
                    /* task id也是整数 */
                    int task_id_int = json_object_get_int(task_id_obj);
                    snprintf((*tasks)[*count].job_id, sizeof((*tasks)[*count].job_id), "%s", job_id);
                    snprintf((*tasks)[*count].task_id, sizeof((*tasks)[*count].task_id), "%d", task_id_int);
                    strncpy((*tasks)[*count].file_id, json_object_get_string(file_id_obj), sizeof((*tasks)[*count].file_id) - 1);
                    
                    /* 提取filename（如果有的话） */
                    if (json_object_object_get_ex(task, "filename", &filename_obj)) {
                        const char *fname = json_object_get_string(filename_obj);
                        if (fname) {
                            strncpy((*tasks)[*count].filename, fname, sizeof((*tasks)[*count].filename) - 1);
                        } else {
                            strncpy((*tasks)[*count].filename, json_object_get_string(file_id_obj), sizeof((*tasks)[*count].filename) - 1);
                        }
                    } else {
                        strncpy((*tasks)[*count].filename, json_object_get_string(file_id_obj), sizeof((*tasks)[*count].filename) - 1);
                    }
                    
                    printf("  Task: id=%s, fileId=%s, filename=%s\n", (*tasks)[*count].task_id, (*tasks)[*count].file_id, (*tasks)[*count].filename);
                    
                    (*count)++;
                }
            }
        }
    }
    
    json_object_put(root);
    free(response);
    return 0;
}

/*
 * 下载文件到本地
 * 根据fileId从服务器下载文件内容，保存到本地
 * 使用GET请求，参数格式: /api/files/getFile?data=["fileId"]
 * @param client HTTP客户端
 * @param file_id 文件ID（用于API请求）
 * @param filename 文件名（用于保存到本地）
 * @param local_path 输出：本地文件路径
 * @param path_size 路径缓冲区大小
 * @return 0成功，-1失败
 */
int download_file_to_local(HttpClient *client, const char *file_id, const char *filename, char *local_path, size_t path_size) {
    /* 确保下载目录存在 */
    if (ensure_download_folder() != 0) {
        printf("Failed to create download folder\n");
        return -1;
    }
    
    /* 构建本地文件路径，使用原始文件名 */
    snprintf(local_path, path_size, DOWNLOAD_FOLDER "/%s", filename);
    
    /* 构建文件下载URL */
    char url[512];
    snprintf(url, sizeof(url), "%s?data=[\"%s\"]", API_GET_FILE, file_id);
    
    char *response = NULL;
    long status_code = 0;
    
    /* 下载文件 */
    const char *cookie = http_client_get_cookie(client);
    int ret = http_get_with_cookie(client, url, cookie, &response, &status_code);
    
    if (ret != 0 || status_code != 200 || !response) {
        printf("Failed to download file %s (status: %ld)\n", file_id, status_code);
        return -1;
    }
    
    /* 写入本地文件 */
    FILE *fp = fopen(local_path, "wb");
    if (!fp) {
        printf("Failed to create local file: %s\n", local_path);
        free(response);
        return -1;
    }
    
    size_t written = fwrite(response, 1, strlen(response), fp);
    fclose(fp);
    free(response);
    
    if (written == 0) {
        printf("Failed to write file\n");
        return -1;
    }
    
    printf("File downloaded: %s (%zu bytes)\n", local_path, written);
    return 0;
}

/*
 * 下载并打印文件（兼容旧接口）
 * 先下载到本地，然后打印
 */
int download_and_print_file(HttpClient *client, const char *file_id) {
    char local_path[MAX_PATH];
    
    /* 下载到本地（使用file_id作为文件名） */
    if (download_file_to_local(client, file_id, file_id, local_path, sizeof(local_path)) != 0) {
        return -1;
    }
    
    /* 打印文件 */
    return print_file_to_default_printer(local_path);
}

/*
 * 报告打印任务成功
 * 通知服务器某个任务已打印完成
 * 使用POST请求，body格式: [taskId]
 */
int report_task_succeeded(HttpClient *client, const char *task_id) {
    /* 构建请求body，格式为JSON数组 */
    char json_body[256];
    snprintf(json_body, sizeof(json_body), "[%s]", task_id);
    
    char *response = NULL;
    long status_code = 0;
    
    /* 发送POST请求 */
    int ret = http_post_with_client_cookie(client, API_TASK_SUCCEED, json_body, &response, &status_code);
    
    if (ret == 0 && status_code == 200) {
        printf("Task %s reported as succeeded\n", task_id);
    } else {
        printf("Failed to report task success (status: %ld)\n", status_code);
    }
    
    if (response) free(response);
    
    return ret;
}
