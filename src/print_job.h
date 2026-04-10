/*
 * 打印任务头文件
 * 
 * 功能说明:
 * - 从服务器获取待打印的任务
 * - 下载打印文件到本地
 * - 调用Windows打印API打印文件
 * - 通知服务器打印完成
 * - 枚举本地打印机
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

/*
 * 本地打印机信息结构体
 */
typedef struct _LocalPrinterInfo {
    char name[256];          /* 打印机名称 (UTF-8) */
    wchar_t wname[256];      /* 打印机名称 (宽字符) */
    char port[256];          /* 端口名称 */
    char driver[256];        /* 驱动名称 */
    int enabled;             /* 是否在服务器上注册 */
} LocalPrinterInfo;

/*
 * 打印机列表结构体
 */
typedef struct _PrinterList {
    LocalPrinterInfo *printers;  /* 打印机数组 */
    int count;                   /* 打印机数量 */
} PrinterList;

/*
 * 计算机信息结构体
 */
typedef struct _ComputerInfo {
    char id[256];          /* 计算机ID */
    char name[256];        /* 计算机名称 */
    char **printers;      /* 服务器上注册的打印机名称数组 */
    int printer_count;    /* 打印机数量 */
} ComputerInfo;

/* ==================== 函数声明 ==================== */
/*
 * 枚举本地打印机
 * @param list 输出：打印机列表
 * @return 0成功，-1失败
 */
int enum_local_printers(PrinterList *list);

/*
 * 释放打印机列表内存
 * @param list 打印机列表
 */
void free_printer_list(PrinterList *list);

/*
 * 获取计算机信息
 * @param client HTTP客户端
 * @param computer_id 计算机ID
 * @param info 输出：计算机信息
 * @return 0成功，-1失败，-2计算机不存在
 */
int get_computer_info(HttpClient *client, const char *computer_id, ComputerInfo *info);

/*
 * 释放计算机信息内存
 * @param info 计算机信息
 */
void free_computer_info(ComputerInfo *info);

/*
 * 设置计算机名称
 * @param client HTTP客户端
 * @param computer_id 计算机ID
 * @param new_name 新名称
 * @return 0成功，-1失败
 */
int set_computer_name(HttpClient *client, const char *computer_id, const char *new_name);

/*
 * 添加计算机
 * @param client HTTP客户端
 * @param computer_id 计算机ID
 * @param computer_name 计算机名称
 * @return 0成功，-1失败
 */
int add_computer(HttpClient *client, const char *computer_id, const char *computer_name);

/*
 * 添加打印机到计算机
 * @param client HTTP客户端
 * @param computer_id 计算机ID
 * @param printer_name 打印机名称
 * @return 0成功，-1失败
 */
int add_computer_printer(HttpClient *client, const char *computer_id, const char *printer_name);

/*
 * 从计算机删除打印机
 * @param client HTTP客户端
 * @param computer_id 计算机ID
 * @param printer_name 打印机名称
 * @return 0成功，-1失败
 */
int remove_computer_printer(HttpClient *client, const char *computer_id, const char *printer_name);

/*
 * 打印文件到默认打印机
 * @param file_path 文件路径
 * @return 0成功，-1失败
 */
int print_file_to_default_printer(const char *file_path);

/*
 * 获取等待中的打印任务列表
 * @param client HTTP客户端
 * @param computer_id 计算机ID（用于过滤）
 * @param tasks 输出：任务数组指针
 * @param count 输出：任务数量
 * @return 0成功，-1失败
 */
int get_waiting_print_jobs(HttpClient *client, const char *computer_id, PrintTaskInfo **tasks, int *count);

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
