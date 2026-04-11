/*
 * 打印任务实现
 * 
 * 功能说明:
 * - 从服务器获取待打印的任务
 * - 通知服务器打印完成
 */

#include "print_job.h"
#include "http_client.h"
#include "config.h"
#include "ui.h"
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 获取等待中的打印任务列表
 * 从服务器API获取待打印任务
 * 使用POST请求，body为"[]"
 */
int get_waiting_print_jobs(HttpClient *client, const char *computer_id, PrintTaskInfo **tasks, int *count) {
    char *response = NULL;
    long status_code = 0;
    
    /* 构建过滤参数JSON */
    char json_body[512];
    if (computer_id && computer_id[0] != '\0') {
        snprintf(json_body, sizeof(json_body), 
            "[{\"state\": \"waiting_print\", \"computerId\": \"%s\"}]", 
            computer_id);
    } else {
        snprintf(json_body, sizeof(json_body), 
            "[{\"state\": \"waiting_print\"}]");
    }
    
    /* 发送带Cookie的POST请求 */
    int ret = http_post_with_client_cookie(client, API_LIST_PRINTJOBS, json_body, &response, &status_code);
    
    if (ret != 0 || status_code != 200 || !response) {
        add_log(L"获取待打印任务失败");
        return -1;
    }
    
    /* 解析JSON响应 */
    json_object *root = parse_json_response(response);
    if (!root) {
        add_log(L"解析JSON响应失败");
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
            add_log(L"响应中没有printJobs字段");
            json_object_put(root);
            free(response);
            *count = 0;
            return 0;
        }
    }
    
    int array_len = json_object_array_length(jobs_array);
    if (array_len == 0) {
        add_log(L"没有找到打印任务");
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
            
            wchar_t wide_job_id[256];
            MultiByteToWideChar(CP_UTF8, 0, job_id, -1, wide_job_id, 256);
            
            wchar_t log[256];
            swprintf(log, 256, L"任务 %s 有 %d 个打印任务", wide_job_id, task_count);
            add_log(log);
            
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
        wchar_t wide_task_id[256];
        MultiByteToWideChar(CP_UTF8, 0, task_id, -1, wide_task_id, 256);
        
        wchar_t log[256];
        swprintf(log, 256, L"任务 %s 已上报成功", wide_task_id);
        add_log(log);
    } else {
        wchar_t wide_task_id[256];
        MultiByteToWideChar(CP_UTF8, 0, task_id, -1, wide_task_id, 256);
        
        wchar_t log[256];
        swprintf(log, 256, L"上报任务状态失败 %s (状态码: %ld)", wide_task_id, status_code);
        add_log(log);
    }
    
    if (response) free(response);
    
    return ret;
}
