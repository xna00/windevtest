/*
 * UI 头文件
 * 
 * 功能说明:
 * - Tab控件管理
 * - 日志显示
 * - 打印机设置界面
 */

#ifndef UI_H
#define UI_H

#include <windows.h>
#include "http_client.h"
#include "print_job.h"

/* ==================== 控件ID ==================== */
#define ID_BTN_SAVE_NAME 100
#define ID_BTN_ENABLE_PRINTER 101
#define ID_BTN_DISABLE_PRINTER 102

/* ==================== 全局控件句柄 ==================== */
extern HWND g_log_static;           /* 日志列表框 */
extern HWND g_tab_ctrl;             /* Tab控件 */
extern HWND g_printer_name_edit;    /* 计算机名称输入框 */
extern HWND g_printer_list_view;    /* 打印机列表 */
extern HWND g_btn_save_name;        /* 保存按钮 */
extern HWND g_btn_enable;           /* 启用按钮 */
extern HWND g_btn_disable;          /* 禁用按钮 */
extern HWND g_static_computer_name; /* 计算机名称标签 */
extern HWND g_static_printer_list;  /* 打印机列表标签 */

/* ==================== 外部变量 ==================== */
extern HttpClient *g_http_client;
extern char g_computer_id[256];
extern PrinterList g_local_printers;
extern ComputerInfo g_computer_info;

/* ==================== 函数声明 ==================== */

/*
 * 初始化UI控件
 * 在WM_CREATE中调用，创建所有子窗口
 */
void init_ui_controls(HWND hwnd, HINSTANCE hInst);

/*
 * 处理Tab切换
 * 在WM_NOTIFY的TCN_SELCHANGE中调用
 */
void on_tab_changed(HWND hwnd);

/*
 * 添加日志消息
 * @param msg 日志内容（宽字符）
 */
void add_log(const wchar_t *msg);

/*
 * 初始化打印机设置Tab
 * 创建计算机名称编辑框、打印机列表等
 */
void init_printer_tab(void);

/*
 * 刷新打印机列表
 * 从服务器获取计算机信息，枚举本地打印机，对比显示
 */
void refresh_printer_list(void);

/*
 * 保存计算机名称
 * 点击保存按钮时调用
 */
void on_save_computer_name(void);

/*
 * 启用打印机
 * 点击启用按钮时调用
 */
void on_enable_printer(void);

/*
 * 禁用打印机
 * 点击禁用按钮时调用
 */
void on_disable_printer(void);

/*
 * 刷新打印机列表（后台线程入口）
 * 避免阻塞UI线程
 */
void refresh_printer_list_thread(void *arg);

/*
 * 处理按钮点击
 * @param hwnd 窗口句柄
 * @param button_id 按钮ID
 */
void handle_button_click(HWND hwnd, int button_id);

#endif
