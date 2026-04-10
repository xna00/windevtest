/*
 * 设备ID实现
 * 
 * 功能说明:
 * - 从注册表读取 MachineGuid 作为电脑唯一ID
 * - 获取电脑名称
 */

#include "device_id.h"
#include <stdio.h>
#include <string.h>

/*
 * 获取电脑唯一ID
 * 从注册表 HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Cryptography 读取 MachineGuid
 * 无需管理员权限
 */
int get_device_id(char *id, size_t id_size) {
    HKEY hKey;
    DWORD data_size = (DWORD)id_size;
    LONG result;
    wchar_t wbuffer[256];
    
    /* 打开注册表项 */
    result = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Cryptography",
        0,
        KEY_READ,
        &hKey
    );
    
    if (result != ERROR_SUCCESS) {
        /* 尝试备用路径 */
        result = RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Cryptography\\MachineGuid",
            0,
            KEY_READ,
            &hKey
        );
        
        if (result != ERROR_SUCCESS) {
            return -1;
        }
    }
    
    /* 读取 MachineGuid (wide string) */
    data_size = sizeof(wbuffer);
    result = RegQueryValueExW(
        hKey,
        L"MachineGuid",
        NULL,
        NULL,
        (LPBYTE)wbuffer,
        &data_size
    );
    
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS) {
        return -1;
    }
    
    /* 转换为UTF-8 */
    int len = WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, id, (int)id_size, NULL, NULL);
    if (len <= 0) {
        return -1;
    }
    
    return 0;
}

/*
 * 获取电脑名称
 * 使用 GetComputerNameW API
 */
int get_computer_name(char *name, size_t name_size) {
    wchar_t wname[256];
    DWORD size = sizeof(wname) / sizeof(wchar_t);
    
    if (!GetComputerNameW(wname, &size)) {
        return -1;
    }
    
    /* 转换为UTF-8 */
    int len = WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, (int)name_size, NULL, NULL);
    if (len <= 0) {
        return -1;
    }
    
    return 0;
}
