/*
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
#include "http_client.h"
#include <stdio.h>
#include <string.h>
#include <commctrl.h>
#include <process.h>

#pragma comment(lib, "comctl32.lib")

#define WM_REFRESH_PRINTER_LIST (WM_USER + 100)

HWND g_log_static = NULL;
HWND g_tab_ctrl = NULL;
HWND g_printer_name_edit = NULL;
HWND g_printer_list_view = NULL;
HWND g_btn_save_name = NULL;
HWND g_btn_enable = NULL;
HWND g_btn_disable = NULL;
HWND g_static_computer_name = NULL;
HWND g_static_printer_list = NULL;

PrinterList g_local_printers = {NULL, 0};
ComputerInfo g_computer_info = {0};

extern HttpClient *g_http_client;
extern char g_computer_name[256];

void add_log(const wchar_t *msg) {
    if (g_log_static && msg) {
        int index = SendMessageW(g_log_static, LB_ADDSTRING, 0, (LPARAM)msg);
        SendMessageW(g_log_static, LB_SETCURSEL, (WPARAM)index, 0);
    }
}

void init_ui_controls(HWND hwnd, HINSTANCE hInst) {
    g_tab_ctrl = CreateWindowW(WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_RAGGEDRIGHT,
        0, 0, 600, 600,
        hwnd, NULL, hInst, NULL);
    
    TCITEMW tci = {0};
    tci.mask = TCIF_TEXT;
    tci.pszText = L"日志";
    tci.cchTextMax = 3;
    SendMessageW(g_tab_ctrl, TCM_INSERTITEMW, 0, (LPARAM)&tci);
    
    tci.pszText = L"打印机设置";
    tci.cchTextMax = 6;
    SendMessageW(g_tab_ctrl, TCM_INSERTITEMW, 1, (LPARAM)&tci);
    
    g_log_static = CreateWindowW(L"LISTBOX", L"", 
        WS_CHILD | WS_VSCROLL | WS_BORDER | LBS_DISABLENOSCROLL, 
        10, 30, 560, 540, g_tab_ctrl, NULL, hInst, NULL);
    
    ShowWindow(g_log_static, SW_SHOW);
    add_log(L"程序已启动");
}

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
        
        UpdateWindow(hwnd);
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

void init_printer_tab(void) {
    g_static_computer_name = CreateWindowW(L"STATIC", L"计算机名称：",
        WS_CHILD,
        20, 55, 120, 20, g_tab_ctrl, NULL, NULL, NULL);
    
    g_printer_name_edit = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        140, 53, 230, 24, g_tab_ctrl, NULL, NULL, NULL);
    
    g_btn_save_name = CreateWindowW(L"BUTTON", L"保存",
        WS_CHILD | BS_PUSHBUTTON,
        380, 53, 80, 24, GetParent(g_tab_ctrl), (HMENU)ID_BTN_SAVE_NAME, NULL, NULL);
    
    g_static_printer_list = CreateWindowW(L"STATIC", L"打印机列表：",
        WS_CHILD,
        20, 95, 100, 20, g_tab_ctrl, NULL, NULL, NULL);
    
    g_printer_list_view = CreateWindowW(WC_LISTVIEWW, L"",
        WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
        20, 120, 500, 380, g_tab_ctrl, NULL, NULL, NULL);
    
    ListView_SetExtendedListViewStyle(g_printer_list_view, LVS_EX_FULLROWSELECT);
    
    LVCOLUMNW lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.pszText = L"打印机名称";
    lvc.cchTextMax = 6;
    lvc.cx = 240;
    SendMessageW(g_printer_list_view, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvc);
    
    lvc.pszText = L"状态";
    lvc.cchTextMax = 3;
    lvc.cx = 120;
    SendMessageW(g_printer_list_view, LVM_INSERTCOLUMNW, 1, (LPARAM)&lvc);
    
    lvc.pszText = L"操作";
    lvc.cchTextMax = 3;
    lvc.cx = 120;
    SendMessageW(g_printer_list_view, LVM_INSERTCOLUMNW, 2, (LPARAM)&lvc);
    
    g_btn_disable = CreateWindowW(L"BUTTON", L"禁用",
        WS_CHILD | BS_PUSHBUTTON,
        20, 510, 100, 28, GetParent(g_tab_ctrl), (HMENU)ID_BTN_DISABLE_PRINTER, NULL, NULL);
    
    g_btn_enable = CreateWindowW(L"BUTTON", L"启用",
        WS_CHILD | BS_PUSHBUTTON,
        130, 510, 100, 28, GetParent(g_tab_ctrl), (HMENU)ID_BTN_ENABLE_PRINTER, NULL, NULL);
}

void refresh_printer_list(void) {
    if (g_computer_id[0] != '\0') {
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
        
        for (int i = 0; i < g_local_printers.count; i++) {
            g_local_printers.printers[i].enabled = 0;
            for (int j = 0; j < g_computer_info.printer_count; j++) {
                if (strcmp(g_local_printers.printers[i].name, g_computer_info.printers[j]) == 0) {
                    g_local_printers.printers[i].enabled = 1;
                    break;
                }
            }
        }
        
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
        
        ComputerInfo new_info = {0};
        int ret = get_computer_info(g_http_client, g_computer_id, &new_info);
        
        if (ret == 0) {
            free_computer_info(&g_computer_info);
            memcpy(&g_computer_info, &new_info, sizeof(ComputerInfo));
            
            if (g_computer_info.name[0] != '\0') {
                wchar_t wname[256];
                MultiByteToWideChar(CP_UTF8, 0, g_computer_info.name, -1, wname, 256);
                SetWindowTextW(g_printer_name_edit, wname);
                strncpy(g_computer_name, g_computer_info.name, sizeof(g_computer_name) - 1);
            }
            
            for (int i = 0; i < g_local_printers.count; i++) {
                g_local_printers.printers[i].enabled = 0;
                for (int j = 0; j < g_computer_info.printer_count; j++) {
                    if (strcmp(g_local_printers.printers[i].name, g_computer_info.printers[j]) == 0) {
                        g_local_printers.printers[i].enabled = 1;
                        break;
                    }
                }
            }
            
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
                
                get_computer_info(g_http_client, g_computer_id, &g_computer_info);
            }
        }
    } else {
        char computer_name[256] = {0};
        get_computer_name(computer_name, sizeof(computer_name));
        wchar_t wname[256];
        MultiByteToWideChar(CP_UTF8, 0, computer_name, -1, wname, 256);
        SetWindowTextW(g_printer_name_edit, wname);
        strncpy(g_computer_name, computer_name, sizeof(g_computer_name) - 1);
        
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

void refresh_printer_list_thread(void *arg) {
    Sleep(100);
    refresh_printer_list();
    _endthread();
}

void on_save_computer_name(void) {
    add_log(L"保存按钮被点击");
    
    if (g_computer_id[0] == '\0') {
        add_log(L"未找到计算机ID");
        return;
    }
    
    wchar_t wname[256];
    GetWindowTextW(g_printer_name_edit, wname, 256);
    
    wchar_t debug[512];
    swprintf(debug, 512, L"编辑框内容: %s", wname);
    add_log(debug);
    
    char name[256];
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, 256, NULL, NULL);
    
    wchar_t cid[256];
    swprintf(cid, 256, L"计算机ID: %S", g_computer_id);
    add_log(cid);
    
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

void on_enable_printer(void) {
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
    
    if (add_computer_printer(g_http_client, g_computer_id, g_local_printers.printers[sel].name) == 0) {
        add_log(L"打印机已启用");
        free_computer_info(&g_computer_info);
        refresh_printer_list();
    } else {
        add_log(L"启用打印机失败");
    }
}

void on_disable_printer(void) {
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
    
    if (remove_computer_printer(g_http_client, g_computer_id, g_local_printers.printers[sel].name) == 0) {
        add_log(L"打印机已禁用");
        free_computer_info(&g_computer_info);
        refresh_printer_list();
    } else {
        add_log(L"禁用打印机失败");
    }
}

void handle_button_click(HWND hwnd, int button_id) {
    (void)hwnd;
    
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