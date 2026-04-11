﻿/*
 * UI 实现
 * 
 * 功能说明:
 * - Tab控件管理
 * - 日志显示
 * - 打印机设置界面
 * 
 * 本文件包含所有UI相关的函数实现
 */

#include "ui.h"
#include "device_id.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <commctrl.h>
#include <process.h>  /* _endthread */

#pragma comment(lib, "comctl32.lib")

/* ==================== Tab切换消息 ==================== */
#define WM_REFRESH_PRINTER_LIST (WM_USER + 100)

/* ==================== 全局控件句柄 ==================== */
/* 这些变量在main.c中声明，在main.c的WM_CREATE中初始化 */
HWND g_log_static = NULL;           /* 日志列表框 */
HWND g_tab_ctrl = NULL;             /* Tab控件 */
HWND g_printer_name_edit = NULL;    /* 计算机名称输入框 */
HWND g_printer_list_view = NULL;    /* 打印机列表 */
HWND g_btn_save_name = NULL;        /* 保存按钮 */
HWND g_btn_enable = NULL;            /* 启用按钮 */
HWND g_btn_disable = NULL;          /* 禁用按钮 */
HWND g_static_computer_name = NULL; /* 计算机名称标签 */
HWND g_static_printer_list = NULL;  /* 打印机列表标签 */

/* ==================== 本地打印机和计算机信息 ==================== */
PrinterList g_local_printers = {NULL, 0};
static ComputerInfo g_computer_info = {0};

/* ==================== 外部变量 ==================== */
/* HttpClient在main.c中初始化 */
extern HttpClient *g_http_client;
extern char g_computer_id[256];
extern char g_computer_name[256];

/* ==================== 添加日志（UTF-8）==================== */
/**
 * @brief 向日志列表添加一行文本
 * @param msg 日志内容（UTF-8编码的字符串）
 * @return void 无返回值
 * 
 * @note 该函数将UTF-8字符串转换为宽字符后添加到ListBox控件
 *       自动滚动到最新添加的行
 */
void add_log(const wchar_t *msg) {
    if (g_log_static && msg) {
        int index = SendMessageW(g_log_static, LB_ADDSTRING, 0, (LPARAM)msg);
        SendMessageW(g_log_static, LB_SETCURSEL, (WPARAM)index, 0);
    }
}

/* ==================== 初始化Tab和日志控件 ==================== */
/**
 * @brief 初始化Tab控件和日志列表
 * @param hwnd 父窗口句柄
 * @param hInst 程序实例句柄
 * @return void 无返回值
 * 
 * @details 创建Tab控件和日志ListBox
 * @note 在WM_CREATE消息中调用
 * 
 * 窗口结构:
 * ┌─────────────────────────────────────┐
 * │ Tab: [Log] [Printer Settings]       │ ← Tab控件
 * ├─────────────────────────────────────┤
 * │                                     │
 * │         日志列表框                  │ ← ListBox
 * │                                     │
 * │                                     │
 * └─────────────────────────────────────┘
 */
void init_ui_controls(HWND hwnd, HINSTANCE hInst) {
    /*
     * 创建Tab控件
     * WC_TABCONTROLW = "SysTabControl32"
     * WS_CLIPSIBLINGS = 防止子控件相互覆盖
     */
    g_tab_ctrl = CreateWindowW(WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_RAGGEDRIGHT,
        0, 0, 600, 600,
        hwnd, NULL, hInst, NULL);
    
    /* 添加Tab标签 */
    TCITEMW tci = {0};
    tci.mask = TCIF_TEXT;
    tci.pszText = L"日志";
    tci.cchTextMax = 3;
    SendMessageW(g_tab_ctrl, TCM_INSERTITEMW, 0, (LPARAM)&tci);
    
    tci.pszText = L"打印机设置";
    tci.cchTextMax = 6;
    SendMessageW(g_tab_ctrl, TCM_INSERTITEMW, 1, (LPARAM)&tci);
    
    /*
     * 创建日志列表框
     * WS_VSCROLL = 垂直滚动条
     * WS_BORDER = 边框
     * LBS_DISABLENOSCROLL = 禁用时也显示滚动条
     */
    g_log_static = CreateWindowW(L"LISTBOX", L"", 
        WS_CHILD | WS_VSCROLL | WS_BORDER | LBS_DISABLENOSCROLL, 
        10, 30, 560, 540, g_tab_ctrl, NULL, hInst, NULL);
    
    ShowWindow(g_log_static, SW_SHOW);
    add_log(L"程序已启动");
}

/* ==================== 处理Tab切换 ==================== */
/**
 * @brief 处理Tab切换事件
 * @param hwnd 窗口句柄
 * @return void 无返回值
 * 
 * @details 当用户点击Tab时调用，根据当前选中的Tab显示/隐藏相应控件
 * - Tab 0: 显示日志列表
 * - Tab 1: 显示打印机设置界面，并触发刷新打印机列表
 * 
 * @note 在WM_NOTIFY的TCN_SELCHANGE消息中调用
 */
void on_tab_changed(HWND hwnd) {
    int cur_sel = TabCtrl_GetCurSel(g_tab_ctrl);
    
    if (cur_sel == 1) {
        ShowWindow(g_log_static, SW_HIDE);
        ShowWindow(g_static_computer_name, SW_SHOW);
        ShowWindow(g_printer_name_edit, SW_SHOW);
        ShowWindow(g_btn_save_name, SW_SHOW);
        ShowWindow(g_static_printer_list, SW_SHOW);
        ShowWindow(g_printer_list_view, SW_SHOW);
        ShowWindow(g_btn_disable, SW_SHOW);
        ShowWindow(g_btn_enable, SW_SHOW);
        
        /* 强制立即重绘控件 */
        UpdateWindow(hwnd);
        
        /* 异步刷新打印机列表，避免阻塞UI */
        PostMessageW(hwnd, WM_REFRESH_PRINTER_LIST, 0, 0);
    } else {
        ShowWindow(g_static_computer_name, SW_HIDE);
        ShowWindow(g_printer_name_edit, SW_HIDE);
        ShowWindow(g_btn_save_name, SW_HIDE);
        ShowWindow(g_static_printer_list, SW_HIDE);
        ShowWindow(g_printer_list_view, SW_HIDE);
        ShowWindow(g_btn_disable, SW_HIDE);
        ShowWindow(g_btn_enable, SW_HIDE);
        ShowWindow(g_log_static, SW_SHOW);
    }
}

/* ==================== 初始化打印机设置Tab ==================== */
/*
 * 初始化打印机设置界面
 * 创建计算机名称编辑框和打印机列表
 * 
 * 界面布局:
 * ┌─────────────────────────────────────────┐
 * │ Computer Name: [_______________] [Save]  │
 * │                                          │
 * │ Printer List:                            │
 * │ ┌──────────────┬──────────┬─────────┐ │
 * │ │ Printer Name │ Status   │ Action  │ │
 * │ ├──────────────┼──────────┼─────────┤ │
 * │ │ HP Printer   │ Enabled  │Disable  │ │
 * │ └──────────────┴──────────┴─────────┘ │
 * │                                          │
 * │              [Disable] [Enable]            │
 * └─────────────────────────────────────────┘
 */
/**
 * @brief 初始化打印机设置界面
 * @param void 无参数
 * @return void 无返回值
 * 
 * @details 创建计算机名称编辑框、按钮、打印机ListView等控件
 * @note 控件初始隐藏，由on_tab_changed控制显示/隐藏
 */
void init_printer_tab(void) {
    /* 创建静态文本标签：计算机名称 */
    g_static_computer_name = CreateWindowW(L"STATIC", L"计算机名称：",
        WS_CHILD,  /* 初始隐藏 */
        20, 55, 120, 20, g_tab_ctrl, NULL, NULL, NULL);
    
    /* 创建编辑框：计算机名称 */
    g_printer_name_edit = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        140, 53, 230, 24, g_tab_ctrl, NULL, NULL, NULL);
    
    /* 创建按钮：保存 - 父窗口是主窗口，以便接收WM_COMMAND消息 */
    g_btn_save_name = CreateWindowW(L"BUTTON", L"保存",
        WS_CHILD | BS_PUSHBUTTON,
        380, 53, 80, 24, GetParent(g_tab_ctrl), (HMENU)ID_BTN_SAVE_NAME, NULL, NULL);
    
    /* 创建静态文本标签：打印机列表 */
    g_static_printer_list = CreateWindowW(L"STATIC", L"打印机列表：",
        WS_CHILD,
        20, 95, 100, 20, g_tab_ctrl, NULL, NULL, NULL);
    
    /*
     * 创建列表视图（ListView）
     * LVS_REPORT = 报表模式（多列）
     * LVS_SINGLESEL = 单选
     */
    g_printer_list_view = CreateWindowW(WC_LISTVIEWW, L"",
        WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
        20, 120, 500, 380, g_tab_ctrl, NULL, NULL, NULL);
    
    /* 设置扩展样式：整行选中 */
    ListView_SetExtendedListViewStyle(g_printer_list_view, LVS_EX_FULLROWSELECT);
    
    /* 添加列：打印机名称 */
    LVCOLUMNW lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.pszText = L"打印机名称";
    lvc.cchTextMax = 6;
    lvc.cx = 240;
    SendMessageW(g_printer_list_view, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvc);
    
    /* 添加列：状态 */
    lvc.pszText = L"状态";
    lvc.cchTextMax = 3;
    lvc.cx = 120;
    SendMessageW(g_printer_list_view, LVM_INSERTCOLUMNW, 1, (LPARAM)&lvc);
    
    /* 添加列：操作 */
    lvc.pszText = L"操作";
    lvc.cchTextMax = 3;
    lvc.cx = 120;
    SendMessageW(g_printer_list_view, LVM_INSERTCOLUMNW, 2, (LPARAM)&lvc);
    
    /* 创建按钮：禁用 - 父窗口是主窗口 */
    g_btn_disable = CreateWindowW(L"BUTTON", L"禁用",
        WS_CHILD | BS_PUSHBUTTON,
        20, 510, 100, 28, GetParent(g_tab_ctrl), (HMENU)ID_BTN_DISABLE_PRINTER, NULL, NULL);
    
    /* 创建按钮：启用 - 父窗口是主窗口 */
    g_btn_enable = CreateWindowW(L"BUTTON", L"启用",
        WS_CHILD | BS_PUSHBUTTON,
        130, 510, 100, 28, GetParent(g_tab_ctrl), (HMENU)ID_BTN_ENABLE_PRINTER, NULL, NULL);
}

/* ==================== 刷新打印机列表 ==================== */
/**
 * @brief 刷新打印机列表
 * @param void 无参数
 * @return void 无返回值
 * 
 * @details 刷新打印机列表的完整流程:
 * 1. 获取本地计算机名称，填入编辑框
 * 2. 枚举本地打印机（EnumPrinters API）
 * 3. 从服务器获取计算机信息（computerInfo API）
 * 4. 如果计算机不存在，调用addComputer注册
 * 5. 对比本地打印机和服务器上的打印机，设置启用/禁用状态
 * 6. 更新ListView显示
 */
void refresh_printer_list(void) {
    /* 打印机列表已在启动时获取，这里不再重新枚举 */
    
    /* 步骤1: 如果已登录，先使用缓存的计算机信息快速显示 */
    if (g_computer_id[0] != '\0') {
        /* 先用缓存数据显示界面 */
        if (g_computer_info.name[0] != '\0') {
            wchar_t wname[256];
            MultiByteToWideChar(CP_UTF8, 0, g_computer_info.name, -1, wname, 256);
            SetWindowTextW(g_printer_name_edit, wname);
            strncpy(g_computer_name, g_computer_info.name, sizeof(g_computer_name) - 1);
        } else {
            char computer_name[256] = {0};
            get_computer_name(computer_name, sizeof(computer_name));
            wchar_t wname[256];
            MultiByteToWideChar(CP_UTF8, 0, computer_name, -1, wname, 256);
            SetWindowTextW(g_printer_name_edit, wname);
            strncpy(g_computer_name, computer_name, sizeof(g_computer_name) - 1);
        }
        
        /* 对比本地打印机和缓存的服务器打印机 */
        for (int i = 0; i < g_local_printers.count; i++) {
            g_local_printers.printers[i].enabled = 0;
            for (int j = 0; j < g_computer_info.printer_count; j++) {
                if (strcmp(g_local_printers.printers[i].name, g_computer_info.printers[j]) == 0) {
                    g_local_printers.printers[i].enabled = 1;
                    break;
                }
            }
        }
        
        /* 步骤2: 更新ListView显示 */
        ListView_DeleteAllItems(g_printer_list_view);
        
        for (int i = 0; i < g_local_printers.count; i++) {
            LVITEMW item = {0};
            item.mask = LVIF_TEXT;
            item.iItem = i;
            
            item.iSubItem = 0;
            item.pszText = g_local_printers.printers[i].wname;
            item.cchTextMax = (int)wcslen(g_local_printers.printers[i].wname) + 1;
            SendMessageW(g_printer_list_view, LVM_INSERTITEMW, 0, (LPARAM)&item);
            
            item.iSubItem = 1;
            item.pszText = g_local_printers.printers[i].enabled ? L"已启用" : L"已禁用";
            item.cchTextMax = 4;
            SendMessageW(g_printer_list_view, LVM_SETITEMW, 0, (LPARAM)&item);
            
            item.iSubItem = 2;
            item.pszText = g_local_printers.printers[i].enabled ? L"禁用" : L"启用";
            item.cchTextMax = 3;
            SendMessageW(g_printer_list_view, LVM_SETITEMW, 0, (LPARAM)&item);
        }
        
        /* 步骤3: 异步从服务器获取最新数据并更新 */
        ComputerInfo new_info = {0};
        int ret = get_computer_info(g_http_client, g_computer_id, &new_info);
        
        if (ret == 0) {
            /* 更新缓存 */
            free_computer_info(&g_computer_info);
            memcpy(&g_computer_info, &new_info, sizeof(ComputerInfo));
            
            /* 如果服务器有名称，更新编辑框 */
            if (g_computer_info.name[0] != '\0') {
                wchar_t wname[256];
                MultiByteToWideChar(CP_UTF8, 0, g_computer_info.name, -1, wname, 256);
                SetWindowTextW(g_printer_name_edit, wname);
                strncpy(g_computer_name, g_computer_info.name, sizeof(g_computer_name) - 1);
            }
            
            /* 重新对比并更新ListView */
            for (int i = 0; i < g_local_printers.count; i++) {
                g_local_printers.printers[i].enabled = 0;
                for (int j = 0; j < g_computer_info.printer_count; j++) {
                    if (strcmp(g_local_printers.printers[i].name, g_computer_info.printers[j]) == 0) {
                        g_local_printers.printers[i].enabled = 1;
                        break;
                    }
                }
            }
            
            /* 更新ListView中的状态列 */
            for (int i = 0; i < g_local_printers.count; i++) {
                LVITEMW item = {0};
                item.mask = LVIF_TEXT;
                item.iItem = i;
                
                item.iSubItem = 1;
                item.pszText = g_local_printers.printers[i].enabled ? L"已启用" : L"已禁用";
                item.cchTextMax = 4;
                SendMessageW(g_printer_list_view, LVM_SETITEMW, 0, (LPARAM)&item);
                
                item.iSubItem = 2;
                item.pszText = g_local_printers.printers[i].enabled ? L"禁用" : L"启用";
                item.cchTextMax = 3;
                SendMessageW(g_printer_list_view, LVM_SETITEMW, 0, (LPARAM)&item);
            }
        } else if (ret == -2) {
            /* 计算机不存在，需要注册 */
            char name[256];
            get_computer_name(name, sizeof(name));
            if (add_computer(g_http_client, g_computer_id, name) == 0) {
                add_log(L"计算机已注册");
                
                wchar_t default_printer[256];
                DWORD buf_size = sizeof(default_printer) / sizeof(wchar_t);
                if (GetDefaultPrinterW(default_printer, &buf_size)) {
                    char printer_name[256];
                    WideCharToMultiByte(CP_UTF8, 0, default_printer, -1, printer_name, 256, NULL, NULL);
                    if (add_computer_printer(g_http_client, g_computer_id, printer_name) == 0) {
                        wchar_t log[256];
                        swprintf(log, 256, L"默认打印机已添加: %s", default_printer);
                        add_log(log);
                    }
                }
                
                /* 重新获取计算机信息 */
                get_computer_info(g_http_client, g_computer_id, &g_computer_info);
            }
        }
    } else {
        /* 未登录时，使用本地计算机名称 */
        char computer_name[256] = {0};
        get_computer_name(computer_name, sizeof(computer_name));
        wchar_t wname[256];
        MultiByteToWideChar(CP_UTF8, 0, computer_name, -1, wname, 256);
        SetWindowTextW(g_printer_name_edit, wname);
        strncpy(g_computer_name, computer_name, sizeof(g_computer_name) - 1);
        
        /* 更新ListView显示 */
        ListView_DeleteAllItems(g_printer_list_view);
        
        for (int i = 0; i < g_local_printers.count; i++) {
            LVITEMW item = {0};
            item.mask = LVIF_TEXT;
            item.iItem = i;
            
            item.iSubItem = 0;
            item.pszText = g_local_printers.printers[i].wname;
            item.cchTextMax = (int)wcslen(g_local_printers.printers[i].wname) + 1;
            SendMessageW(g_printer_list_view, LVM_INSERTITEMW, 0, (LPARAM)&item);
            
            item.iSubItem = 1;
            item.pszText = L"已禁用";
            item.cchTextMax = 4;
            SendMessageW(g_printer_list_view, LVM_SETITEMW, 0, (LPARAM)&item);
            
            item.iSubItem = 2;
            item.pszText = L"启用";
            item.cchTextMax = 3;
            SendMessageW(g_printer_list_view, LVM_SETITEMW, 0, (LPARAM)&item);
        }
    }
}

/* ==================== 刷新打印机列表（后台线程）==================== */
/**
 * @brief 刷新打印机列表的后台线程入口
 * @param arg 线程参数（未使用）
 * @return void 无返回值（线程自动结束）
 * 
 * @details 在新线程中执行refresh_printer_list，避免阻塞UI线程
 */
void refresh_printer_list_thread(void *arg) {
    Sleep(100);  /* 等待UI创建完成 */
    refresh_printer_list();
    _endthread();
}

/* ==================== 保存计算机名称 ==================== */
/**
 * @brief 保存计算机名称到服务器
 * @param void 无参数
 * @return void 无返回值
 * 
 * @details 点击"保存"按钮时调用，读取编辑框内容并调用API保存
 */
void on_save_computer_name(void) {
    add_log(L"保存按钮被点击");
    
    if (g_computer_id[0] == '\0') {
        add_log(L"未找到计算机ID");
        return;
    }
    
    /* 获取编辑框中的内容 */
    wchar_t wname[256];
    GetWindowTextW(g_printer_name_edit, wname, 256);
    
    wchar_t debug[512];
    swprintf(debug, 512, L"编辑框内容: %s", wname);
    add_log(debug);
    
    /* 转换为UTF-8 */
    char name[256];
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, 256, NULL, NULL);
    
    wchar_t cid[256];
    swprintf(cid, 256, L"计算机ID: %S", g_computer_id);
    add_log(cid);
    
    /* 调用API保存 */
    add_log(L"正在调用API...");
    int result = set_computer_name(g_http_client, g_computer_id, name);
    
    wchar_t result_log[128];
    swprintf(result_log, 128, L"API返回: %d", result);
    add_log(result_log);
    
    if (result == 0) {
        add_log(L"计算机名称已保存");
        strncpy(g_computer_name, name, sizeof(g_computer_name) - 1);
    } else {
        add_log(L"保存计算机名称失败");
    }
}

/* ==================== 启用打印机 ==================== */
/*
 * 启用打印机
 * 将本地打印机添加到服务器计算机
 */
void on_enable_printer(void) {
    /* 获取选中的打印机 */
    int sel = ListView_GetNextItem(g_printer_list_view, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= g_local_printers.count) {
        add_log(L"请先选择打印机");
        return;
    }
    
    if (g_computer_id[0] == '\0') {
        add_log(L"未找到计算机ID");
        return;
    }
    
    if (g_local_printers.printers[sel].enabled) {
        add_log(L"打印机已启用");
        return;
    }
    
    /* 调用API添加打印机 */
    if (add_computer_printer(g_http_client, g_computer_id, g_local_printers.printers[sel].name) == 0) {
        add_log(L"打印机已启用");
        free_computer_info(&g_computer_info);
        refresh_printer_list();
    } else {
        add_log(L"启用打印机失败");
    }
}

/* ==================== 禁用打印机 ==================== */
/*
 * 禁用打印机
 * 从服务器计算机中删除打印机
 */
void on_disable_printer(void) {
    /* 获取选中的打印机 */
    int sel = ListView_GetNextItem(g_printer_list_view, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= g_local_printers.count) {
        add_log(L"请先选择打印机");
        return;
    }
    
    if (g_computer_id[0] == '\0') {
        add_log(L"未找到计算机ID");
        return;
    }
    
    if (!g_local_printers.printers[sel].enabled) {
        add_log(L"打印机已禁用");
        return;
    }
    
    /* 调用API删除打印机 */
    if (remove_computer_printer(g_http_client, g_computer_id, g_local_printers.printers[sel].name) == 0) {
        add_log(L"打印机已禁用");
        free_computer_info(&g_computer_info);
        refresh_printer_list();
    } else {
        add_log(L"禁用打印机失败");
    }
}

/* ==================== 处理按钮点击 ==================== */
/*
 * 处理按钮点击事件
 */
void handle_button_click(HWND hwnd, int button_id) {
    (void)hwnd;  /* 未使用 */
    
    switch (button_id) {
        case ID_BTN_SAVE_NAME:
            on_save_computer_name();
            break;
        case ID_BTN_ENABLE_PRINTER:
            on_enable_printer();
            break;
        case ID_BTN_DISABLE_PRINTER:
            on_disable_printer();
            break;
    }
}
