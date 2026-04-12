/*
 * 设备ID头文件
 * 
 * 功能说明:
 * - 获取电脑唯一ID (MachineGuid)
 * - 获取电脑名称
 */

#ifndef DEVICE_ID_H
#define DEVICE_ID_H

#include <windows.h>

/*
 * 获取电脑唯一ID
 * 从注册表读取 MachineGuid
 * @param id 输出：设备ID字符串
 * @param id_size 缓冲区大小
 * @return 0成功，-1失败
 */
int get_device_id(char *id, size_t id_size);

/*
 * 获取电脑名称
 * 调用 Windows API 获取计算机名
 * @param name 输出：电脑名称
 * @param name_size 缓冲区大小
 * @return 0成功，-1失败
 */
int get_computer_name(char *name, size_t name_size);

#endif
