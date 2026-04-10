/*
 * 登录对话框头文件
 * 
 * 功能说明:
 * - 提供用户名密码输入对话框
 * - 调用登录API进行身份验证
 * - 返回登录结果给调用者
 */

#ifndef LOGIN_DLG_H
#define LOGIN_DLG_H

#include <windows.h>

/* ==================== 控件ID定义 ==================== */
#define IDC_USERNAME 100  /* 用户名输入框ID */
#define IDC_PASSWORD 101  /* 密码输入框ID */

/* ==================== 数据结构 ==================== */
/*
 * 登录结果结构体
 * 包含登录是否成功、用户名和消息
 */
typedef struct {
    char username[256];      /* 登录成功的用户名 */
    char password[256];      /* 密码（通常不保存） */
    int success;             /* 登录是否成功 */
    wchar_t message[256];    /* 状态消息（UTF-16） */
} LoginResult;

/* ==================== 函数声明 ==================== */
/*
 * 登录对话框窗口回调函数
 */
INT_PTR CALLBACK LoginDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/*
 * 获取登录结果
 * 在对话框关闭后调用，获取登录状态
 */
LoginResult* get_login_result(void);

#endif
