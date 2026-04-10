/*
 * 登录对话框实现
 * 
 * 功能说明:
 * - 显示用户名密码输入对话框
 * - 调用登录API进行身份验证
 * - 解析服务器响应，提取token
 */

#define CURL_STATICLIB
#include "login_dlg.h"
#include "http_client.h"
#include "config.h"
#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <winuser.h>

/* 外部变量：HTTP客户端（由main.c提供） */
extern HttpClient *g_http_client;

/* 登录结果（全局变量，对话框关闭后由main.c读取） */
static LoginResult g_login_result;

/*
 * 获取登录结果
 * 对话框关闭后，main.c调用此函数获取结果
 */
LoginResult* get_login_result(void) {
    return &g_login_result;
}

/*
 * 登录对话框窗口回调函数
 * 处理对话框的消息和用户输入
 */
INT_PTR CALLBACK LoginDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG:
            /* 对话框初始化 */
            g_login_result.success = 0;
            memset(&g_login_result, 0, sizeof(g_login_result));
            return TRUE;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                /* 用户点击了"登录"按钮 */
                
                /* 获取用户名和密码（宽字符） */
                wchar_t wusername[256] = {0};
                wchar_t wpassword[256] = {0};
                
                GetDlgItemTextW(hwnd, IDC_USERNAME, wusername, sizeof(wusername) / sizeof(wchar_t));
                GetDlgItemTextW(hwnd, IDC_PASSWORD, wpassword, sizeof(wpassword) / sizeof(wchar_t));
                
                /* 验证输入不为空 */
                if (wcslen(wusername) == 0 || wcslen(wpassword) == 0) {
                    MessageBoxW(hwnd, L"Please enter username and password", L"Error", MB_OK | MB_ICONERROR);
                    return TRUE;
                }
                
                /* 转换字符编码：UTF-16 -> UTF-8 */
                char username[256] = {0};
                char password[256] = {0};
                WideCharToMultiByte(CP_UTF8, 0, wusername, -1, username, sizeof(username), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wpassword, -1, password, sizeof(password), NULL, NULL);
                
                /* 构建JSON请求体（数组格式） */
                char json_body[512];
                snprintf(json_body, sizeof(json_body), 
                    "[{\"username\":\"%s\",\"password\":\"%s\"}]", username, password);
                
                /* 发送登录请求 */
                char *response = NULL;
                long status_code = 0;
                
                int ret = http_post_with_body(g_http_client, API_LOGIN, json_body, &response, &status_code);
                
                /* 处理响应 */
                if (ret == 0 && status_code == 200 && response) {
                    json_object *root = parse_json_response(response);
                    if (root) {
                        json_object *token_obj;
                        json_object *username_obj;
                        
                        /* 检查响应中是否有token和username字段 */
                        if (json_object_object_get_ex(root, "token", &token_obj) &&
                            json_object_object_get_ex(root, "username", &username_obj)) {
                            
                            /* 登录成功 */
                            g_login_result.success = 1;
                            strncpy(g_login_result.username, username, sizeof(g_login_result.username) - 1);
                            wcscpy(g_login_result.message, L"Login successful");
                            
                            /* 从响应中提取token，保存到Cookie */
                            const char *token = json_object_get_string(token_obj);
                            if (token) {
                                char cookie[512];
                                snprintf(cookie, sizeof(cookie), "token=%s", token);
                                http_client_set_cookie(g_http_client, cookie);
                            }
                            
                            EndDialog(hwnd, IDOK);  /* 关闭对话框，返回IDOK */
                        } else {
                            /* 登录失败，尝试获取错误消息 */
                            json_object *msg_obj;
                            if (json_object_object_get_ex(root, "message", &msg_obj)) {
                                const char *msg = json_object_get_string(msg_obj);
                                MultiByteToWideChar(CP_UTF8, 0, msg, -1, g_login_result.message, sizeof(g_login_result.message) / sizeof(wchar_t));
                            } else {
                                wcscpy(g_login_result.message, L"Login failed");
                            }
                            MessageBoxW(hwnd, g_login_result.message, L"Login Failed", MB_OK | MB_ICONERROR);
                        }
                        json_object_put(root);
                    }
                    free(response);
                } else {
                    /* 网络请求失败 */
                    wcscpy(g_login_result.message, L"Connection failed");
                    MessageBoxW(hwnd, L"Login request failed", L"Error", MB_OK | MB_ICONERROR);
                }
                
                return TRUE;
            }
            else if (LOWORD(wParam) == IDCANCEL) {
                /* 用户点击了"取消"按钮 */
                wcscpy(g_login_result.message, L"Login cancelled");
                EndDialog(hwnd, IDCANCEL);  /* 关闭对话框，返回IDCANCEL */
                return TRUE;
            }
            break;
    }
    return FALSE;  /* 未处理的消息返回FALSE，让系统继续处理 */
}
