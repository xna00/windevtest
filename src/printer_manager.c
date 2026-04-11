/*
 * 打印机管理实现
 * 
 * 功能说明:
 * - 枚举本地打印机
 * - 管理打印机列表
 */

#include "printer_manager.h"
#include <winspool.h>
#include <stdlib.h>
#include <string.h>

/*
 * 枚举本地打印机
 * 使用 EnumPrinters API 获取所有本地打印机
 */
int enum_local_printers(PrinterList *list) {
    DWORD buffer_size = 0;
    DWORD count = 0;
    DWORD needed = 0;
    
    /* 第一次调用：获取所需缓冲区大小
     * EnumPrintersW - 枚举可用的打印机
     * 参数1: 枚举标志，PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS 表示枚举本地和连接的打印机
     * 参数2: 打印机名称，NULL表示枚举所有打印机
     * 参数3: 打印机信息级别，2表示PRINTER_INFO_2结构
     * 参数4: 接收打印机信息的缓冲区，NULL表示仅获取所需大小
     * 参数5: 缓冲区大小，0表示仅获取所需大小
     * 参数6: 指向变量的指针，该变量接收所需的缓冲区大小
     * 参数7: 指向变量的指针，该变量接收枚举的打印机数量
     * 返回值: 如果函数成功，返回非零值；如果失败，返回零
     */
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
    
    /* 第二次调用：获取打印机列表
     * EnumPrintersW - 枚举可用的打印机
     * 参数1: 枚举标志
     * 参数2: 打印机名称
     * 参数3: 打印机信息级别
     * 参数4: 接收打印机信息的缓冲区
     * 参数5: 缓冲区大小
     * 参数6: 指向变量的指针，该变量接收所需的缓冲区大小
     * 参数7: 指向变量的指针，该变量接收枚举的打印机数量
     * 返回值: 如果函数成功，返回非零值；如果失败，返回零
     */
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
            
            /* 转换为UTF-8
             * WideCharToMultiByte - 将宽字符字符串转换为多字节字符串
             * 参数1: 代码页，CP_UTF8表示使用UTF-8编码
             * 参数2: 转换标志，0表示默认行为
             * 参数3: 要转换的宽字符字符串
             * 参数4: 宽字符字符串的长度，-1表示自动计算
             * 参数5: 接收多字节字符串的缓冲区
             * 参数6: 缓冲区大小
             * 参数7: 替代字符，NULL表示使用默认值
             * 参数8: 指向变量的指针，指示是否使用了替代字符，NULL表示不需要
             * 返回值: 如果函数成功，返回写入缓冲区的字节数；如果失败，返回零
             */
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