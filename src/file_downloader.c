/*
 * 文件下载器实现
 * 
 * 功能说明:
 * - 下载文件到本地
 * - 管理下载目录
 * - 处理文件路径
 */

#include "file_downloader.h"
#include "http_client.h"
#include "config.h"
#include "ui.h"
#include "print_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <direct.h>

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
    strncpy_s(safe_filename, sizeof(safe_filename), filename, _TRUNCATE);
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
    
    /* 将UTF-8路径转换为UTF-16
     * MultiByteToWideChar - 将多字节字符串转换为宽字符字符串
     * 参数1: 代码页，CP_UTF8表示使用UTF-8编码
     * 参数2: 转换标志，0表示默认行为
     * 参数3: 要转换的多字节字符串
     * 参数4: 多字节字符串的长度，-1表示自动计算
     * 参数5: 接收宽字符字符串的缓冲区
     * 参数6: 缓冲区大小
     * 返回值: 如果函数成功，返回写入缓冲区的字符数；如果失败，返回零
     */
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
    
    /* 将UTF-8路径转换为宽字符串
     * MultiByteToWideChar - 将多字节字符串转换为宽字符字符串
     * 参数1: 代码页，CP_UTF8表示使用UTF-8编码
     * 参数2: 转换标志，0表示默认行为
     * 参数3: 要转换的多字节字符串
     * 参数4: 多字节字符串的长度，-1表示自动计算
     * 参数5: 接收宽字符字符串的缓冲区
     * 参数6: 缓冲区大小
     * 返回值: 如果函数成功，返回写入缓冲区的字符数；如果失败，返回零
     */
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