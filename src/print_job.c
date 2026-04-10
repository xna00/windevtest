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
#include "ui.h"
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
        add_log(L"获取默认打印机失败");
        return -1;
    }
    
    /* 打开打印机 */
    HANDLE hPrinter;
    if (!OpenPrinterW(printer_name, &hPrinter, NULL)) {
        add_log(L"打开打印机失败");
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
        add_log(L"开始打印文档失败");
        ClosePrinter(hPrinter);
        return -1;
    }
    
    /* 开始一页 */
    if (!StartPagePrinter(hPrinter)) {
        add_log(L"开始打印页失败");
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return -1;
    }
    
    /* 将UTF-8路径转换为宽字符串 */
    wchar_t wide_file_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, file_path, -1, wide_file_path, MAX_PATH);
    
    /* 读取文件内容并打印 */
    FILE *fp = _wfopen(wide_file_path, L"rb");
    if (!fp) {
        add_log(L"打开文件失败");
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
        add_log(L"内存分配失败");
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
        add_log(L"读取文件不完整");
        free(buffer);
        EndPagePrinter(hPrinter);
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return -1;
    }
    
    /* 发送打印数据 */
    DWORD written;
    if (!WritePrinter(hPrinter, (LPVOID)buffer, (DWORD)file_size, &written)) {
        add_log(L"写入打印机失败");
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
    
    wchar_t log[128];
    swprintf(log, 128, L"打印任务已提交 (%ld 字节)", file_size);
    add_log(log);
    return 0;
}

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
 * 清理文件名，移除或替换不合法的字符
 */
static void sanitize_filename(char *filename) {
    /* 替换文件系统不允许的字符 */
    const char *invalid = "\\/:*?\"<>|";
    char *p = filename;
    while (*p) {
        if (strchr(invalid, *p)) {
            *p = '_';
        }
        p++;
    }
}

/*
 * 下载文件到本地
 * 根据fileId从服务器下载文件内容，保存到本地
 * 使用GET请求，参数格式: /api/files/getFile?data=["fileId"]
 * @param client HTTP客户端
 * @param file_id 文件ID（用于API请求）
 * @param filename 文件名（用于保存到本地，UTF-8编码）
 * @param local_path 输出：本地文件路径
 * @param path_size 路径缓冲区大小
 * @return 0成功，-1失败
 */
int download_file_to_local(HttpClient *client, const char *file_id, const char *filename, char *local_path, size_t path_size) {
    /* 确保下载目录存在 */
    if (ensure_download_folder() != 0) {
        add_log(L"创建下载目录失败");
        return -1;
    }
    
    /* 构建本地文件路径（使用.ps后缀） */
    char safe_filename[512];
    strncpy(safe_filename, filename, sizeof(safe_filename) - 1);
    safe_filename[sizeof(safe_filename) - 1] = '\0';
    sanitize_filename(safe_filename);
    
    /* 移除原始后缀，添加.ps后缀 */
    char *dot = strrchr(safe_filename, '.');
    if (dot) {
        *dot = '\0';
    }
    snprintf(local_path, path_size, DOWNLOAD_FOLDER "/%s.ps", safe_filename);
    
    /* 构建文件下载URL（使用PS文件API） */
    char url[512];
    snprintf(url, sizeof(url), "%s?data=[\"%s\"]", API_GET_PS_FILE, file_id);
    
    char *response = NULL;
    long status_code = 0;
    size_t data_size = 0;
    
    /* 下载文件（二进制数据） */
    const char *cookie = http_client_get_cookie(client);
    int ret = http_get_binary(client, url, cookie, &response, &data_size, &status_code);
    
    if (ret != 0 || status_code != 200 || !response) {
        wchar_t wide_file_id[256];
        MultiByteToWideChar(CP_UTF8, 0, file_id, -1, wide_file_id, 256);
        
        wchar_t log[512];
        swprintf(log, 512, L"下载文件失败 %s (状态码: %ld)", wide_file_id, status_code);
        add_log(log);
        if (response) free(response);
        return -1;
    }
    
    /* 检查响应是否包含错误信息（可能是JSON格式的错误） */
    if (data_size > 0 && (response[0] == '{' || response[0] == '[')) {
        /* 看起来是JSON响应，不是文件内容 */
        add_log(L"服务器返回JSON而非文件内容");
        free(response);
        return -1;
    }
    
    /* 将UTF-8路径转换为UTF-16 */
    wchar_t wide_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, local_path, -1, wide_path, MAX_PATH);
    
    /* 使用_wfopen打开文件（支持UTF-16路径） */
    FILE *fp = _wfopen(wide_path, L"wb");
    if (!fp) {
        add_log(L"创建本地文件失败");
        free(response);
        return -1;
    }
    
    /* 使用实际的data_size，不依赖\0终止符 */
    size_t written = fwrite(response, 1, data_size, fp);
    fclose(fp);
    free(response);
    
    if (written == 0 || written != data_size) {
        wchar_t log[128];
        swprintf(log, 128, L"文件写入不完整 (已写入 %zu/%zu 字节)", written, data_size);
        add_log(log);
        return -1;
    }
    
    /* 将UTF-8路径转换为宽字符串 */
    wchar_t wide_path2[256];
    MultiByteToWideChar(CP_UTF8, 0, local_path, -1, wide_path2, 256);
    
    wchar_t log[512];
    swprintf(log, 512, L"文件已下载: %s (%zu 字节)", wide_path2, written);
    add_log(log);
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

/*
 * 枚举本地打印机
 * 使用 EnumPrinters API 获取所有本地打印机
 */
int enum_local_printers(PrinterList *list) {
    DWORD buffer_size = 0;
    DWORD count = 0;
    DWORD needed = 0;
    
    /* 第一次调用：获取所需缓冲区大小 */
    EnumPrintersW(
        PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
        NULL,
        2,
        NULL,
        0,
        &needed,
        &count
    );
    
    if (needed == 0) {
        list->printers = NULL;
        list->count = 0;
        return 0;
    }
    
    /* 分配缓冲区 */
    LPBYTE buffer = malloc(needed);
    if (!buffer) return -1;
    
    /* 第二次调用：获取打印机列表 */
    if (!EnumPrintersW(
        PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
        NULL,
        2,
        buffer,
        needed,
        &needed,
        &count
    )) {
        free(buffer);
        return -1;
    }
    
    /* 分配打印机数组 */
    list->printers = malloc(sizeof(LocalPrinterInfo) * count);
    if (!list->printers) {
        free(buffer);
        return -1;
    }
    
    /* 解析打印机信息 */
    PRINTER_INFO_2W *info = (PRINTER_INFO_2W *)buffer;
    list->count = 0;
    
    for (DWORD i = 0; i < count; i++) {
        /* 只处理本地打印机和网络打印机 */
        if (info[i].pPrinterName && info[i].pPrinterName[0] != L'\0') {
            /* 保存宽字符版本 */
            wcscpy(list->printers[list->count].wname, info[i].pPrinterName);
            
            /* 转换为UTF-8 */
            WideCharToMultiByte(CP_UTF8, 0, info[i].pPrinterName, -1,
                list->printers[list->count].name, 256, NULL, NULL);
            
            if (info[i].pPortName) {
                WideCharToMultiByte(CP_UTF8, 0, info[i].pPortName, -1,
                    list->printers[list->count].port, 256, NULL, NULL);
            } else {
                list->printers[list->count].port[0] = '\0';
            }
            
            if (info[i].pDriverName) {
                WideCharToMultiByte(CP_UTF8, 0, info[i].pDriverName, -1,
                    list->printers[list->count].driver, 256, NULL, NULL);
            } else {
                list->printers[list->count].driver[0] = '\0';
            }
            
            list->printers[list->count].enabled = 0;
            list->count++;
        }
    }
    
    free(buffer);
    return 0;
}

/*
 * 释放打印机列表内存
 */
void free_printer_list(PrinterList *list) {
    if (list->printers) {
        free(list->printers);
        list->printers = NULL;
    }
    list->count = 0;
}

/*
 * 获取计算机信息
 * @return 0成功，-1失败，-2计算机不存在
 */
int get_computer_info(HttpClient *client, const char *computer_id, ComputerInfo *info) {
    char json_body[512];
    snprintf(json_body, sizeof(json_body), "[\"%s\"]", computer_id);
    
    char *response = NULL;
    long status_code = 0;
    
    int ret = http_post_with_client_cookie(client, API_COMPUTER_INFO, json_body, &response, &status_code);
    
    if (ret != 0) {
        add_log(L"HTTP请求失败");
        return -1;
    }
    
    wchar_t status_log[128];
    swprintf(status_log, 128, L"API返回状态码: %ld", status_code);
    add_log(status_log);
    
    if (response) {
        add_log(L"收到响应");
    } else {
        add_log(L"无响应");
    }
    
    if (status_code == 404) {
        add_log(L"计算机未找到 (404)");
        free(response);
        return -2;
    }
    
    if (status_code == 400) {
        if (response) {
            json_object *root = parse_json_response(response);
            if (root) {
                json_object *error_obj;
                if (json_object_object_get_ex(root, "errorCode", &error_obj)) {
                    const char *error_code = json_object_get_string(error_obj);
                    if (error_code && strcmp(error_code, "ENTITY_NOT_FOUND") == 0) {
                        add_log(L"计算机未找到 (ENTITY_NOT_FOUND)");
                        json_object_put(root);
                        free(response);
                        return -2;
                    }
                }
                json_object_put(root);
            }
            free(response);
        }
        return -1;
    }
    
    if (status_code != 200 || !response) {
        add_log(L"意外的状态码或无响应");
        free(response);
        return -1;
    }
    
    /* 解析JSON响应 */
    json_object *root = parse_json_response(response);
    if (!root) {
        free(response);
        return -1;
    }
    
    /* 初始化 */
    memset(info, 0, sizeof(ComputerInfo));
    strncpy(info->id, computer_id, sizeof(info->id) - 1);
    
    /* 获取name字段 */
    json_object *name_obj;
    if (json_object_object_get_ex(root, "name", &name_obj)) {
        const char *name = json_object_get_string(name_obj);
        if (name) {
            strncpy(info->name, name, sizeof(info->name) - 1);
        }
    }
    
    /* 获取printers数组 */
    json_object *printers_array;
    if (json_object_object_get_ex(root, "printers", &printers_array)) {
        info->printer_count = json_object_array_length(printers_array);
        if (info->printer_count > 0) {
            info->printers = malloc(sizeof(char*) * info->printer_count);
            for (int i = 0; i < info->printer_count; i++) {
                json_object *printer = json_object_array_get_idx(printers_array, i);
                json_object *name_obj;
                if (json_object_object_get_ex(printer, "name", &name_obj)) {
                    const char *pname = json_object_get_string(name_obj);
                    info->printers[i] = malloc(256);
                    if (pname) {
                        strncpy(info->printers[i], pname, 255);
                    } else {
                        info->printers[i][0] = '\0';
                    }
                } else {
                    info->printers[i] = malloc(256);
                    info->printers[i][0] = '\0';
                }
            }
        }
    }
    
    json_object_put(root);
    free(response);
    return 0;
}

/*
 * 释放计算机信息内存
 */
void free_computer_info(ComputerInfo *info) {
    if (info->printers) {
        for (int i = 0; i < info->printer_count; i++) {
            if (info->printers[i]) {
                free(info->printers[i]);
            }
        }
        free(info->printers);
        info->printers = NULL;
    }
    info->printer_count = 0;
}

/*
 * 设置计算机名称
 */
int set_computer_name(HttpClient *client, const char *computer_id, const char *new_name) {
    char json_body[512];
    snprintf(json_body, sizeof(json_body), "[\"%s\", \"%s\"]", computer_id, new_name);
    
    char *response = NULL;
    long status_code = 0;
    
    int ret = http_post_with_client_cookie(client, API_SET_COMPUTER_NAME, json_body, &response, &status_code);
    
    if (response) free(response);
    
    return (ret == 0 && status_code == 200) ? 0 : -1;
}

/*
 * 添加计算机
 */
int add_computer(HttpClient *client, const char *computer_id, const char *computer_name) {
    char json_body[512];
    snprintf(json_body, sizeof(json_body), "[\"%s\", \"%s\"]", computer_id, computer_name);
    
    char *response = NULL;
    long status_code = 0;
    
    int ret = http_post_with_client_cookie(client, API_ADD_COMPUTER, json_body, &response, &status_code);
    
    if (response) free(response);
    
    return (ret == 0 && status_code == 200) ? 0 : -1;
}

/*
 * 添加打印机到计算机
 */
int add_computer_printer(HttpClient *client, const char *computer_id, const char *printer_name) {
    char json_body[512];
    snprintf(json_body, sizeof(json_body), "[\"%s\", \"%s\"]", computer_id, printer_name);
    
    char *response = NULL;
    long status_code = 0;
    
    int ret = http_post_with_client_cookie(client, API_ADD_PRINTER, json_body, &response, &status_code);
    
    if (response) free(response);
    
    return (ret == 0 && status_code == 200) ? 0 : -1;
}

/*
 * 从计算机删除打印机
 */
int remove_computer_printer(HttpClient *client, const char *computer_id, const char *printer_name) {
    char json_body[512];
    snprintf(json_body, sizeof(json_body), "[\"%s\", \"%s\"]", computer_id, printer_name);
    
    char *response = NULL;
    long status_code = 0;
    
    int ret = http_post_with_client_cookie(client, API_REMOVE_PRINTER, json_body, &response, &status_code);
    
    if (response) free(response);
    
    return (ret == 0 && status_code == 200) ? 0 : -1;
}
