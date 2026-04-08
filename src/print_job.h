/*
 * 打印任务头文件
 * 
 * 功能说明:
 * - 从服务器获取待打印的任务
 * - 下载打印文件到本地
 * - 调用Windows打印API打印文件
 * - 通知服务器打印完成
 */

#ifndef PRINT_JOB_H
#define PRINT_JOB_H

#include <windows.h>
#include "http_client.h"

/* ==================== 数据结构 ==================== */
/*
 * 打印任务信息结构体
 */
typedef struct _PrintTaskInfo {
    char job_id[256];    /* 打印任务ID */
    char task_id[256];    /* 子任务ID */
    char file_id[256];    /* 文件ID，用于下载文件 */
    char filename[256];   /* 文件名，用于保存到本地 */
} PrintTaskInfo;

/* ==================== 函数声明 ==================== */
/*
 * 打印文件到默认打印机
 * @param file_path 文件路径
 * @return 0成功，-1失败
 */
int print_file_to_default_printer(const char *file_path);

/*
 * 获取等待中的打印任务列表
 * @param client HTTP客户端
 * @param tasks 输出：任务数组指针
 * @param count 输出：任务数量
 * @return 0成功，-1失败
 */
int get_waiting_print_jobs(HttpClient *client, PrintTaskInfo **tasks, int *count);

/*
 * 下载文件到本地
 * @param client HTTP客户端
 * @param file_id 文件ID（用于API请求）
 * @param filename 文件名（用于保存到本地）
 * @param local_path 输出：本地文件路径
 * @param path_size 路径缓冲区大小
 * @return 0成功，-1失败
 */
int download_file_to_local(HttpClient *client, const char *file_id, const char *filename, char *local_path, size_t path_size);

/*
 * 下载并打印文件
 * @param client HTTP客户端
 * @param file_id 文件ID
 * @return 0成功，-1失败
 */
int download_and_print_file(HttpClient *client, const char *file_id);

/*
 * 报告打印任务成功
 * @param client HTTP客户端
 * @param task_id 任务ID
 * @return 0成功，-1失败
 */
int report_task_succeeded(HttpClient *client, const char *task_id);

#endif
