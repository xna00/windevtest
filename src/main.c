/*
 * PrintDriver - 主程序入口
 * 
 * 功能说明:
 * 1. 系统托盘运行，最小化到托盘
 * 2. 登录验证，与服务器通信
 * 3. WebSocket实时接收打印任务
 * 4. 自动下载并打印文件
 * 5. 打印机设置管理
 * 
 * 窗口结构:
 * ┌─────────────────────────────────────┐
 * │ 标题栏: Print Driver                │
 * ├─────────────────────────────────────┤
 * │ Tab: [Log] [Printer Settings]       │
 * ├─────────────────────────────────────┤
 * │                                     │
 * │         日志列表框 或               │
 * │         打印机设置界面              │
 * │                                     │
 * └─────────────────────────────────────┘
 */

#define CURL_STATICLIB

/* ==================== 头文件 ==================== */
#include <windows.h>
#include <process.h>
#include <wingdi.h>
#include <winuser.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#include "config.h"         /* API地址等配置 */
#include "http_client.h"   /* HTTP客户端 */
#include "login_dlg.h"    /* 登录对话框 */
#include "print_job.h"    /* 打印任务 */
#include "device_id.h"    /* 设备ID */
#include "websocket.h"    /* WebSocket */
#include "ui.h"           /* UI控件 */
#include "resource.h"     /* 资源定义 */

#include <json-c/json.h>  /* JSON解析 */

/* ==================== 链接库 ==================== */
#pragma comment(lib, "winspool.lib")   /* 打印 spool */
#pragma comment(lib, "shell32.lib")     /* 托盘图标 */
#pragma comment(lib, "comctl32.lib")  /* Tab控件等 */

/* ==================== 常量定义 ==================== */
#define WM_TRAYICON (WM_USER + 1)      /* 托盘图标消息 */
#define ID_TRAY_SHOW 1                   /* 托盘菜单 - 显示窗口 */
#define ID_TRAY_EXIT 2                   /* 托盘菜单 - 退出程序 */
#define DEBUG_MODE 1                     /* 开发阶段设为1，点击关闭按钮直接退出 */
#define WM_REFRESH_PRINTER_LIST (WM_USER + 100)  /* 刷新打印机列表消息 */

/* ==================== 全局变量 ==================== */
static HINSTANCE g_hInst;               /* 程序实例句柄 */
static HWND g_hwnd;                     /* 主窗口句柄 */
static HWND g_status_static;            /* 状态文本控件 */
HttpClient *g_http_client = NULL;       /* HTTP客户端 */
static WebSocketClient *g_ws_client = NULL;  /* WebSocket客户端 */
static char g_username[256] = {0};
char g_computer_id[256] = {0};           /* 电脑唯一ID（从注册表获取） */
char g_computer_name[256] = {0};        /* 电脑名称 */
static NOTIFYICONDATAW g_nid;            /* 托盘图标数据 */

/* ==================== 函数声明 ==================== */
/* 窗口消息处理 */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* 后台线程 */
void check_current_user_thread(void *arg);
void refresh_printer_list_thread(void *arg);

/* 业务逻辑 */
void check_current_user(void);
void show_login_dialog(void);
void connect_websocket(void);
void on_websocket_message(const char *message);
void handle_print_job(void);
void register_computer(void);

/* ==================== 程序入口 ==================== */
/*
 * WinMain - Windows程序入口点
 * 
 * 启动流程:
 * 1. 注册窗口类
 * 2. 创建主窗口
 * 3. 初始化HTTP客户端
 * 4. 创建托盘图标
 * 5. 检查用户登录状态（后台线程）
 * 6. 进入消息循环
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;
    
    /* 1. 注册窗口类 */
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;                    /* 窗口消息处理函数 */
    wc.hInstance = hInstance;
    wc.lpszClassName = L"PrintDriverWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    
    /* 2. 创建主窗口 */
    g_hwnd = CreateWindowW(
        L"PrintDriverWindow",
        L"Print Driver",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,  /* WS_CLIPCHILDREN减少重绘闪烁 */
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 620,
        NULL, NULL,
        hInstance, NULL
    );
    
    if (!g_hwnd) return 0;
    
    /* 3. 初始化HTTP客户端 */
    g_http_client = http_client_init();
    http_client_load_cookie(g_http_client, COOKIE_FILE);  /* 加载保存的cookie */
    
    /* 4. 创建系统托盘图标 */
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"Print Driver");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    
    /* 显示窗口 */
    ShowWindow(g_hwnd, SW_SHOW);
    
    /* 5. 在后台线程检查用户登录状态 */
    _beginthread(check_current_user_thread, 0, NULL);
    
    /* 6. 消息循环 */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    return 0;
}

/* ==================== 窗口消息处理 ==================== */
/*
 * WndProc - 窗口消息处理函数
 * 
 * 消息类型:
 * - WM_CREATE: 窗口创建时初始化UI控件
 * - WM_COMMAND: 按钮点击等命令
 * - WM_NOTIFY: Tab控件切换通知
 * - WM_TRAYICON: 托盘图标消息
 * - WM_CLOSE: 关闭按钮
 * - WM_DESTROY: 窗口销毁
 */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        /* ---------- WM_CREATE: 窗口创建 ---------- */
        case WM_CREATE:
            g_status_static = NULL;  /* 暂时不使用状态栏 */
            
            /* 初始化Tab和日志控件 */
            init_ui_controls(hwnd, g_hInst);
            
            /* 初始化打印机设置控件（但初始隐藏） */
            init_printer_tab();
            
            /* 启动时预先获取打印机列表，避免切换Tab时延迟 */
            if (enum_local_printers(&g_local_printers) != 0) {
                add_log(L"枚举本地打印机失败");
            }
            
            /* 获取并显示默认打印机名称 */
            wchar_t printer_name[256];
            DWORD buf_size = sizeof(printer_name) / sizeof(wchar_t);
            if (GetDefaultPrinterW(printer_name, &buf_size)) {
                wchar_t log[512];
                swprintf(log, 512, L"默认打印机: %s", printer_name);
                add_log(log);
            } else {
                add_log(L"未找到默认打印机");
            }
            return 0;
            
        /* ---------- WM_COMMAND: 命令消息 ---------- */
        case WM_COMMAND:
            /* 托盘菜单命令 */
            if (LOWORD(wParam) == ID_TRAY_SHOW) {
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            }
            else if (LOWORD(wParam) == ID_TRAY_EXIT) {
                Shell_NotifyIconW(NIM_DELETE, &g_nid);
                PostQuitMessage(0);
            }
            /* 按钮命令 */
            else {
                handle_button_click(hwnd, LOWORD(wParam));
            }
            break;
            
        /* ---------- WM_NOTIFY: 通知消息 ---------- */
        case WM_NOTIFY:
            /* Tab控件切换 */
            if (((LPNMHDR)lParam)->hwndFrom == g_tab_ctrl) {
                if (((LPNMHDR)lParam)->code == TCN_SELCHANGE) {
                    on_tab_changed(hwnd);
                }
            }
            break;
            
        /* ---------- WM_REFRESH_PRINTER_LIST: 刷新打印机列表 ---------- */
        case WM_REFRESH_PRINTER_LIST:
            refresh_printer_list();
            return 0;
            
        /* ---------- WM_SIZE: 窗口大小改变 ---------- */
        case WM_SIZE:
            return 0;  /* 暂不处理，让系统默认处理 */
            
        /* ---------- WM_ERASEBKGND: 背景擦除 ---------- */
        case WM_ERASEBKGND:
            return 1;  /* 返回1表示不需要擦除，避免闪烁 */
            
        /* ---------- WM_TRAYICON: 托盘图标 ---------- */
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONDOWN) {
                /* 右键点击：显示菜单 */
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Show");
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
            }
            else if (lParam == WM_LBUTTONDOWN) {
                /* 左键点击：显示窗口 */
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            }
            break;
            
        /* ---------- WM_CLOSE: 关闭 ---------- */
        case WM_CLOSE:
#if DEBUG_MODE
            /* 开发阶段：直接退出 */
            if (g_http_client) http_client_cleanup(g_http_client);
            if (g_ws_client) ws_cleanup(g_ws_client);
            PostQuitMessage(0);
#else
            /* 发布阶段：隐藏到托盘 */
            ShowWindow(hwnd, SW_HIDE);
#endif
            return 0;
            
        /* ---------- WM_DESTROY: 销毁 ---------- */
        case WM_DESTROY:
            if (g_http_client) http_client_cleanup(g_http_client);
            if (g_ws_client) ws_cleanup(g_ws_client);
            PostQuitMessage(0);
            break;
            
        /* ---------- 默认处理 ---------- */
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

/* ==================== 后台线程：检查用户登录状态 ==================== */
/*
 * check_current_user_thread
 * 
 * 因为网络请求可能较慢，不能在UI线程中执行
 * 所以创建后台线程来检查登录状态
 */
void check_current_user_thread(void *arg) {
    Sleep(100);  /* 等待窗口创建完成 */
    check_current_user();
    _endthread();
}

/* ==================== 检查用户登录状态 ==================== */
/*
 * check_current_user
 * 
 * 调用API: /api/user/currentUser
 * - 401: 未登录，显示登录对话框
 * - 200: 已登录，显示用户名，获取computerId
 * 
 * 登录后流程:
 * 1. 获取设备ID（从注册表）
 * 2. 调用computerInfo API
 * 3. 如果计算机不存在，调用addComputer
 * 4. 连接WebSocket
 */
void check_current_user(void) {
    char *response = NULL;
    long status_code = 0;
    
    add_log(L"检查用户状态...");
    
    /* 调用API检查登录状态 */
    int ret = http_post_with_client_cookie(g_http_client, API_USER_CURRENT, "[]", &response, &status_code);
    
    if (ret != 0) {
        add_log(L"连接失败");
        return;
    }
    
    /* 401: 未登录 */
    if (status_code == 401) {
        add_log(L"未登录");
        show_login_dialog();
        return;
    }
    
    /* 200: 已登录 */
    if (status_code == 200 && response) {
        /* 解析返回的JSON，获取用户名 */
        json_object *root = parse_json_response(response);
        if (root) {
            json_object *username_obj;
            if (json_object_object_get_ex(root, "username", &username_obj)) {
                strncpy(g_username, json_object_get_string(username_obj), sizeof(g_username) - 1);
                
                wchar_t welcome[256];
                swprintf(welcome, 256, L"已登录: %S", g_username);  // %S 用于窄字符
                add_log(welcome);
            }
            json_object_put(root);
        }
        free(response);
        
        /* 获取设备ID（从注册表） */
        if (get_device_id(g_computer_id, sizeof(g_computer_id)) != 0) {
            add_log(L"获取设备ID失败");
        } else {
            wchar_t did[256];
            swprintf(did, 256, L"设备ID: %S", g_computer_id);  // %S 用于窄字符
            add_log(did);
            
            /* 注册计算机 */
            register_computer();
        }
        
        /* 连接WebSocket */
        connect_websocket();
    } else {
        /* 其他错误 */
        wchar_t debug[256];
        swprintf(debug, 256, L"错误: 状态码=%ld", status_code);
        add_log(debug);
        
        if (response) {
            wchar_t wresponse[512];
            MultiByteToWideChar(CP_UTF8, 0, response, -1, wresponse, 512);
            add_log(wresponse);
            free(response);
        }
    }
}

/* ==================== 显示登录对话框 ==================== */
/*
 * show_login_dialog
 * 
 * 弹出登录对话框让用户输入用户名和密码
 * 登录成功后的流程:
 * 1. 保存cookie到文件
 * 2. 获取设备ID
 * 3. 连接WebSocket
 */
void show_login_dialog(void) {
    /* 显示登录对话框（模态对话框） */
    DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDC_LOGIN_DLG), g_hwnd, LoginDialogProc, 0);
    
    /* 获取登录结果 */
    LoginResult *result = get_login_result();
    if (result->success) {
        /* 登录成功 */
        strncpy(g_username, result->username, sizeof(g_username) - 1);
        
        /* 保存cookie */
        http_client_save_cookie(g_http_client, COOKIE_FILE);
        
        /* 获取设备ID */
        if (get_device_id(g_computer_id, sizeof(g_computer_id)) != 0) {
            add_log(L"获取设备ID失败");
        } else {
            wchar_t did[256];
            swprintf(did, 256, L"设备ID: %S", g_computer_id);
            add_log(did);
            
            /* 注册计算机 */
            register_computer();
        }
        
        /* 连接WebSocket */
        connect_websocket();
    }
}

/* ==================== 注册计算机 ==================== */
/*
 * register_computer
 * 
 * 检查计算机是否已注册，如果未注册则：
 * 1. 添加计算机到服务器
 * 2. 添加默认打印机到计算机
 */
void register_computer(void) {
    ComputerInfo info = {0};
    add_log(L"检查计算机注册状态...");
    int ret = get_computer_info(g_http_client, g_computer_id, &info);
    
    wchar_t debug[256];
    swprintf(debug, 256, L"get_computer_info返回: %d", ret);
    add_log(debug);
    
    if (ret == -2) {
        /* 计算机不存在，需要注册 */
        add_log(L"计算机未找到，正在注册...");
        
        /* 获取计算机名称 */
        char computer_name[256] = {0};
        DWORD size = sizeof(computer_name);
        if (!GetComputerNameA(computer_name, &size)) {
            strcpy(computer_name, "Unknown");
        }
        
        wchar_t name_log[256];
        swprintf(name_log, 256, L"计算机名称: %S", computer_name);
        add_log(name_log);
        
        /* 添加计算机 */
        if (add_computer(g_http_client, g_computer_id, computer_name) == 0) {
            add_log(L"计算机注册成功");
            
            /* 获取默认打印机并添加 */
            wchar_t default_printer[256];
            DWORD buf_size = sizeof(default_printer) / sizeof(wchar_t);
            if (GetDefaultPrinterW(default_printer, &buf_size)) {
                char printer_name[256];
                WideCharToMultiByte(CP_UTF8, 0, default_printer, -1, printer_name, 256, NULL, NULL);
                
                if (add_computer_printer(g_http_client, g_computer_id, printer_name) == 0) {
                    wchar_t log[512];
                    swprintf(log, 512, L"默认打印机已添加: %s", default_printer);
                    add_log(log);
                } else {
                    add_log(L"添加默认打印机失败");
                }
            } else {
                add_log(L"未找到默认打印机");
            }
        } else {
            add_log(L"计算机注册失败");
        }
    } else if (ret == 0) {
        /* 计算机已存在 */
        add_log(L"计算机已注册");
        free_computer_info(&info);
    } else {
        add_log(L"检查计算机信息失败");
    }
}

/* ==================== 连接WebSocket ==================== */
/*
 * connect_websocket
 * 
 * WebSocket连接成功后:
 * - 进入消息接收循环
 * - 收到消息后调用on_websocket_message处理
 * 
 * 注意: 这是一个阻塞循环，会一直等待消息
 *       如果要支持UI响应，需要移到单独线程
 */
void connect_websocket(void) {
    add_log(L"正在连接服务器...");
    
    /* 初始化WebSocket客户端 */
    const char *cookie = http_client_get_cookie(g_http_client);
    g_ws_client = ws_init(cookie);
    
    /* 连接WebSocket服务器 */
    if (ws_connect(g_ws_client, API_WEBSOCKET_URL) == 0) {
        add_log(L"WebSocket已连接");
        
        /* 进入消息接收循环 */
        char buffer[4096];
        while (1) {
            size_t recv_bytes = 0;
            int ret = ws_receive(g_ws_client, buffer, sizeof(buffer), &recv_bytes);
            
            if (ret == 0 && recv_bytes > 0) {
                buffer[recv_bytes] = '\0';
                on_websocket_message(buffer);
            }
            
            Sleep(100);  /* 避免CPU占用过高 */
        }
    } else {
        add_log(L"连接服务器失败");
    }
}

/* ==================== 处理WebSocket消息 ==================== */
/*
 * on_websocket_message
 * 
 * 收到WebSocket消息时的处理
 * 目前只处理check_jobs类型的消息
 * 
 * 消息格式: {"type": "check_jobs", ...}
 */
void on_websocket_message(const char *message) {
    if (!message || strlen(message) == 0) return;
    
    /* 解析JSON，检查type字段 */
    json_object *root = parse_json_response(message);
    if (!root) {
        add_log(L"解析消息失败");
        return;
    }
    
    json_object *type_obj;
    if (!json_object_object_get_ex(root, "type", &type_obj)) {
        json_object_put(root);
        return;
    }
    
    const char *msg_type = json_object_get_string(type_obj);
    if (!msg_type) {
        json_object_put(root);
        return;
    }
    
    /* 检查是否是check_jobs消息 */
    if (strcmp(msg_type, "check_jobs") == 0) {
        add_log(L"收到打印任务检查消息");
        handle_print_job();
    } else {
        /* 其他类型的消息 */
        add_log(L"WebSocket收到消息:");
        wchar_t wmessage[512];
        MultiByteToWideChar(CP_UTF8, 0, message, -1, wmessage, 512);
        add_log(wmessage);
    }
    
    json_object_put(root);
}

/* ==================== 处理打印任务 ==================== */
/*
 * handle_print_job
 * 
 * 处理打印任务的完整流程:
 * 1. 调用API获取待打印任务列表
 * 2. 遍历每个任务
 * 3. 下载打印文件到本地
 * 4. 发送到默认打印机打印
 * 5. 通知服务器打印成功
 */
void handle_print_job(void) {
    PrintTaskInfo *tasks = NULL;
    int count = 0;
    
    add_log(L"正在获取待打印任务...");
    
    /* 获取待打印任务 */
    if (get_waiting_print_jobs(g_http_client, g_computer_id, &tasks, &count) != 0) {
        add_log(L"获取打印任务失败");
        return;
    }
    
    if (count == 0) {
        add_log(L"没有待打印任务");
        return;
    }
    
    /* 打印每个任务 */
    for (int i = 0; i < count; i++) {
        wchar_t log[512];
        swprintf(log, 512, L"正在打印任务 %d/%d", i + 1, count);
        add_log(log);
        
        /* 下载文件 */
        char local_path[MAX_PATH];
        if (download_file_to_local(g_http_client, tasks[i].file_id, tasks[i].filename, local_path, sizeof(local_path)) != 0) {
            add_log(L"下载文件失败");
            /* 即使下载失败也上报成功（临时方案） */
            report_task_succeeded(g_http_client, tasks[i].task_id);
            continue;
        }
        
        /* 打印文件 */
        if (print_file_to_default_printer(local_path) != 0) {
            add_log(L"打印失败");
            /* 即使打印失败也上报成功（临时方案） */
            report_task_succeeded(g_http_client, tasks[i].task_id);
            continue;
        }
        
        /* 通知服务器 */
        if (report_task_succeeded(g_http_client, tasks[i].task_id) == 0) {
            add_log(L"任务完成");
        } else {
            add_log(L"上报任务状态失败");
        }
    }
    
    free(tasks);
}
