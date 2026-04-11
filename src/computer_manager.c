/*
 * 计算机管理实现
 * 
 * 功能说明:
 * - 获取计算机信息
 * - 设置计算机名称
 * - 添加计算机
 * - 管理计算机打印机
 */

#include "computer_manager.h"
#include "http_client.h"
#include "config.h"
#include "ui.h"
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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