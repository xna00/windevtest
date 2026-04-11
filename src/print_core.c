/*
 * 核心打印功能实现
 * 
 * 功能说明:
 * - 使用Windows打印API (Winspool) 打印文件
 * - 调用API获取和处理打印任务
 */

#include "print_core.h"
#include "ui.h"
#include <winspool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 打印文件到默认打印机
 * 使用Windows Winspool API进行RAW模式打印
 * @param file_path 文件路径
 * @return 0成功，-1失败
 */
int print_file_to_default_printer(const char *file_path) {
    wchar_t printer_name[256];
    DWORD buf_size = sizeof(printer_name);
    
    /* 获取系统默认打印机名称
     * GetDefaultPrinterW - 获取默认打印机的名称
     * 参数1: 指向接收打印机名称的宽字符缓冲区的指针
     * 参数2: 指向变量的指针，该变量指定缓冲区的大小（以字符为单位）
     * 返回值: 如果函数成功，返回非零值；如果失败，返回零
     */
    if (!GetDefaultPrinterW(printer_name, &buf_size)) {
        add_log(L"获取默认打印机失败");
        return -1;
    }
    
    /* 打开打印机
     * OpenPrinterW - 打开指定的打印机并返回打印机句柄
     * 参数1: 打印机名称
     * 参数2: 指向接收打印机句柄的变量的指针
     * 参数3: 指向PRINTER_DEFAULTS结构的指针，NULL表示使用默认值
     * 返回值: 如果函数成功，返回非零值；如果失败，返回零
     */
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
    
    /* 开始打印文档
     * StartDocPrinterW - 开始一个打印作业
     * 参数1: 打印机句柄
     * 参数2: 文档信息结构的级别
     * 参数3: 指向文档信息结构的指针
     * 返回值: 如果函数成功，返回作业ID；如果失败，返回零
     */
    DWORD job_id = StartDocPrinterW(hPrinter, 1, (LPBYTE)&doc_info);
    if (job_id == 0) {
        add_log(L"开始打印文档失败");
        ClosePrinter(hPrinter);
        return -1;
    }
    
    /* 开始一页
     * StartPagePrinter - 开始打印一页
     * 参数: 打印机句柄
     * 返回值: 如果函数成功，返回非零值；如果失败，返回零
     */
    if (!StartPagePrinter(hPrinter)) {
        add_log(L"开始打印页失败");
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
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
    
    /* 发送打印数据
     * WritePrinter - 向打印机写入数据
     * 参数1: 打印机句柄
     * 参数2: 指向要写入的数据的指针
     * 参数3: 要写入的字节数
     * 参数4: 指向接收实际写入字节数的变量的指针
     * 返回值: 如果函数成功，返回非零值；如果失败，返回零
     */
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
    
    /* 结束一页
     * EndPagePrinter - 结束当前打印页
     * 参数: 打印机句柄
     * 返回值: 如果函数成功，返回非零值；如果失败，返回零
     */
    EndPagePrinter(hPrinter);
    
    /* 结束打印文档
     * EndDocPrinter - 结束打印作业
     * 参数: 打印机句柄
     * 返回值: 如果函数成功，返回非零值；如果失败，返回零
     */
    EndDocPrinter(hPrinter);
    
    /* 关闭打印机
     * ClosePrinter - 关闭打印机句柄
     * 参数: 打印机句柄
     * 返回值: 如果函数成功，返回非零值；如果失败，返回零
     */
    ClosePrinter(hPrinter);
    
    wchar_t log[128];
    swprintf(log, 128, L"打印任务已提交 (%ld 字节)", file_size);
    add_log(log);
    return 0;
}