/*
 * PrintDriver - 打印驱动程序
 * 
 * 功能说明:
 * 1. 系统托盘运行，最小化到托盘
 * 2. 登录验证，与服务器通信
 * 3. WebSocket实时接收打印任务
 * 4. 自动下载并打印文件
 */

#define CURL_STATICLIB
#include <windows.h>
#include <process.h>
#include <wingdi.h>
#include <winuser.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "http_client.h"
#include "login_dlg.h"
#include "print_job.h"
#include "websocket.h"
#include "resource.h"
#include <json-c/json.h>

#pragma comment(lib, "winspool.lib")
#pragma comment(lib, "shell32.lib")

/* ==================== 常量定义 ==================== */
#define WM_TRAYICON (WM_USER + 1)   /* 托盘图标消息 */
#define ID_TRAY_SHOW 1              /* 托盘菜单 - 显示窗口 */
#define ID_TRAY_EXIT 2              /* 托盘菜单 - 退出程序 */
#define DEBUG_MODE 1                /* 开发阶段设为1，点击关闭按钮直接退出 */

/* ==================== 全局变量 ==================== */
static HINSTANCE g_hInst;           /* 程序实例句柄 */
static HWND g_hwnd;                 /* 主窗口句柄 */
static HWND g_status_static;        /* 状态文本控件 */
static HWND g_log_static;           /* 日志文本控件 */
HttpClient *g_http_client = NULL;  /* HTTP客户端 */
static WebSocketClient *g_ws_client = NULL;  /* WebSocket客户端 */
static char g_username[256] = {0};  /* 当前登录用户名 */
static NOTIFYICONDATAW g_nid;       /* 托盘图标数据 */

/* ==================== 函数声明 ==================== */
void set_status(const wchar_t *status);
void add_log(const wchar_t *msg);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void check_current_user_thread(void *arg);
void check_current_user(void);
void show_login_dialog(void);
void connect_websocket(void);
void on_websocket_message(const char *message);
void handle_print_job(void);

/* ==================== 程序入口 ==================== */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;
    
    /* ------------------- 注册窗口类 ------------------- */
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;           /* 窗口消息处理函数 */
    wc.hInstance = hInstance;            /* 程序实例 */
    wc.lpszClassName = L"PrintDriverWindow";  /* 窗口类名 */
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);  /* 默认光标 */
    RegisterClassW(&wc);
    
    /* ------------------- 创建主窗口 ------------------- */
    g_hwnd = CreateWindowW(
        L"PrintDriverWindow",           /* 窗口类名 */
        L"Print Driver",                /* 窗口标题 */
        WS_OVERLAPPEDWINDOW,            /* 窗口样式 */
        CW_USEDEFAULT, CW_USEDEFAULT,   /* 位置：默认 */
        500, 600,                       /* 窗口大小 */
        NULL, NULL,                     /* 父窗口、菜单 */
        hInstance, NULL                 /* 实例、创建参数 */
    );
    
    if (!g_hwnd) return 0;
    
    /* ------------------- 初始化HTTP客户端 ------------------- */
    g_http_client = http_client_init();
    
    /* 加载保存的cookie */
    http_client_load_cookie(g_http_client, COOKIE_FILE);
    
    /* ------------------- 创建系统托盘图标 ------------------- */
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;               /* 接收托盘消息的窗口 */
    g_nid.uID = TRAY_ICON_ID;          /* 图标ID */
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;  /* 自定义消息 */
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"Print Driver");
    Shell_NotifyIconW(NIM_ADD, &g_nid); /* 添加托盘图标 */
    
    /* ------------------- 显示窗口并检查登录状态 ------------------- */
    ShowWindow(g_hwnd, SW_SHOW);
    
    /* 在后台线程检查用户登录状态，避免阻塞UI */
    _beginthread(check_current_user_thread, 0, NULL);
    
    /* ------------------- 消息循环 ------------------- */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}

/* ==================== 窗口消息处理 ==================== */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            /* 创建日志列表框 */
            g_log_static = CreateWindowW(L"LISTBOX", L"", 
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_DISABLENOSCROLL, 
                10, 10, 470, 550, hwnd, NULL, g_hInst, NULL);
            g_status_static = NULL;
            add_log(L"Program started");
            
            /* 获取并显示默认打印机名称 */
            wchar_t printer_name[256];
            DWORD buf_size = sizeof(printer_name) / sizeof(wchar_t);
            if (GetDefaultPrinterW(printer_name, &buf_size)) {
                wchar_t log_msg[512];
                swprintf(log_msg, 512, L"Default printer: %s", printer_name);
                add_log(log_msg);
            } else {
                add_log(L"No default printer found");
            }
            return 0;
            
        case WM_TRAYICON:
            /* 处理托盘图标消息 */
            if (lParam == WM_RBUTTONDOWN) {
                /* 右键点击：显示弹出菜单 */
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
            
        case WM_COMMAND:
            /* 处理菜单命令 */
            if (LOWORD(wParam) == ID_TRAY_SHOW) {
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            }
            else if (LOWORD(wParam) == ID_TRAY_EXIT) {
                Shell_NotifyIconW(NIM_DELETE, &g_nid);  /* 删除托盘图标 */
                PostQuitMessage(0);
            }
            break;
            
        case WM_CLOSE:
#if DEBUG_MODE
            /* 开发阶段：直接退出程序 */
            if (g_http_client) http_client_cleanup(g_http_client);
            if (g_ws_client) ws_cleanup(g_ws_client);
            PostQuitMessage(0);
#else
            /* 发布阶段：隐藏窗口到托盘 */
            ShowWindow(hwnd, SW_HIDE);
#endif
            return 0;
            
        case WM_DESTROY:
            /* 窗口销毁时清理资源 */
            if (g_http_client) http_client_cleanup(g_http_client);
            if (g_ws_client) ws_cleanup(g_ws_client);
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

/* ==================== 后台线程入口 ==================== */
/*
 * 后台线程用于检查用户登录状态
 * 因为网络请求可能较慢，不能在UI线程中执行
 */
void check_current_user_thread(void *arg) {
    Sleep(100);  /* 等待窗口创建完成 */
    check_current_user();
    _endthread();
}

/* ==================== 检查用户登录状态 ==================== */
/*
 * 向服务器发送请求，检查用户是否已登录
 * - 401: 未登录，显示登录对话框
 * - 200: 已登录，显示用户名，连接WebSocket
 * - 其他: 显示错误信息
 */
void check_current_user(void) {
    char *response = NULL;
    long status_code = 0;
    
    add_log(L"Checking user status...");
    set_status(L"Checking user status...");
    
    /* 使用POST请求，发送body '[]' */
    int ret = http_post_with_client_cookie(g_http_client, API_USER_CURRENT, "[]", &response, &status_code);
    
    /* 检查请求是否成功 */
    if (ret != 0) {
        add_log(L"Connection failed");
        set_status(L"Connection failed");
        return;
    }
    
    /* 根据HTTP状态码处理 */
    if (status_code == 401) {
        /* 401表示未授权，需要登录 */
        add_log(L"Not logged in");
        set_status(L"Not logged in");
        show_login_dialog();
    }
    else if (status_code == 200 && response) {
        /* 200表示成功，解析返回的用户信息 */
        json_object *root = parse_json_response(response);
        if (root) {
            json_object *username_obj;
            if (json_object_object_get_ex(root, "username", &username_obj)) {
                strncpy(g_username, json_object_get_string(username_obj), sizeof(g_username) - 1);
                
                char welcome[256];
                snprintf(welcome, sizeof(welcome), "Logged in: %s", g_username);
                wchar_t wwelcome[256];
                MultiByteToWideChar(CP_UTF8, 0, welcome, -1, wwelcome, sizeof(wwelcome) / sizeof(wchar_t));
                add_log(wwelcome);
                set_status(wwelcome);
            }
            json_object_put(root);
        }
        free(response);
        
        /* 已登录，连接WebSocket，服务端会推送check_jobs消息 */
        connect_websocket();
    } else {
        /* 其他状态码，显示服务器返回的错误信息 */
        char debug[256];
        snprintf(debug, sizeof(debug), "Error: status=%ld", status_code);
        wchar_t wdebug[256];
        MultiByteToWideChar(CP_UTF8, 0, debug, -1, wdebug, sizeof(wdebug) / sizeof(wchar_t));
        add_log(wdebug);
        
        if (response) {
            wchar_t wmsg[512];
            MultiByteToWideChar(CP_UTF8, 0, response, -1, wmsg, sizeof(wmsg) / sizeof(wchar_t));
            add_log(wmsg);
            set_status(wmsg);
            free(response);
        }
    }
}

/* ==================== 显示登录对话框 ==================== */
/*
 * 弹出登录对话框让用户输入用户名和密码
 * 登录成功则连接WebSocket
 */
void show_login_dialog(void) {
    DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDC_LOGIN_DLG), g_hwnd, LoginDialogProc, 0);
    
    /* 获取登录结果 */
    LoginResult *result = get_login_result();
    if (result->success) {
        /* 登录成功 */
        strncpy(g_username, result->username, sizeof(g_username) - 1);
        set_status(result->message);
        
        /* 保存cookie到文件 */
        http_client_save_cookie(g_http_client, COOKIE_FILE);
        
        /* 连接WebSocket，服务端会推送check_jobs消息 */
        connect_websocket();
    } else {
        /* 登录失败，显示失败原因 */
        set_status(result->message);
    }
}

/* ==================== 连接WebSocket服务器 ==================== */
/*
 * 与服务器建立WebSocket连接
 * 用于实时接收打印任务通知
 */
void connect_websocket(void) {
    set_status(L"Connecting to server...");
    
    /* 获取登录时保存的Cookie */
    const char *cookie = http_client_get_cookie(g_http_client);
    g_ws_client = ws_init(cookie);
    
    /* 连接WebSocket服务器 */
    if (ws_connect(g_ws_client, API_WEBSOCKET_URL) == 0) {
        add_log(L"WebSocket connected");
        set_status(L"Connected to server");
        
        /* 进入消息接收循环 */
        char buffer[4096];
        while (1) {
            size_t recv_bytes = 0;
            int ret = ws_receive(g_ws_client, buffer, sizeof(buffer), &recv_bytes);
            
            if (ret == 0 && recv_bytes > 0) {
                buffer[recv_bytes] = '\0';
                on_websocket_message(buffer);
            }
            
            Sleep(100);
        }
    } else {
        add_log(L"Failed to connect to server");
        set_status(L"Failed to connect to server");
    }
}

/* ==================== 处理WebSocket消息 ==================== */
/*
 * 收到WebSocket消息时的回调
 * 如果消息类型是 check_jobs，则触发打印任务检查
 */
void on_websocket_message(const char *message) {
    if (message && strlen(message) > 0) {
        /* 解析JSON消息，检查type字段 */
        json_object *root = parse_json_response(message);
        if (root) {
            json_object *type_obj;
            if (json_object_object_get_ex(root, "type", &type_obj)) {
                const char *msg_type = json_object_get_string(type_obj);
                
                if (msg_type && strcmp(msg_type, "check_jobs") == 0) {
                    /* 收到检查任务消息，触发打印任务处理 */
                    add_log(L"Received check_jobs message");
                    handle_print_job();
                } else {
                    /* 其他类型的消息，显示内容 */
                    wchar_t wmsg[1024];
                    MultiByteToWideChar(CP_UTF8, 0, message, -1, wmsg, sizeof(wmsg) / sizeof(wchar_t));
                    add_log(L"WS received:");
                    add_log(wmsg);
                }
            } else {
                /* 没有type字段，显示原始消息 */
                wchar_t wmsg[1024];
                MultiByteToWideChar(CP_UTF8, 0, message, -1, wmsg, sizeof(wmsg) / sizeof(wchar_t));
                add_log(L"WS received:");
                add_log(wmsg);
            }
            json_object_put(root);
        } else {
            /* JSON解析失败，显示原始消息 */
            wchar_t wmsg[1024];
            MultiByteToWideChar(CP_UTF8, 0, message, -1, wmsg, sizeof(wmsg) / sizeof(wchar_t));
            add_log(L"WS received:");
            add_log(wmsg);
        }
    }
}

/* ==================== 处理打印任务 ==================== */
/*
 * 从服务器获取待处理的打印任务
 * 下载文件到本地并发送到打印机打印
 */
void handle_print_job(void) {
    PrintTaskInfo *tasks = NULL;
    int count = 0;
    
    add_log(L"Fetching waiting print jobs...");
    
    /* 获取等待中的打印任务 */
    if (get_waiting_print_jobs(g_http_client, &tasks, &count) == 0) {
        if (count == 0) {
            add_log(L"No waiting print jobs");
            return;
        }
        
        /* 显示找到的任务总数 */
        char log[256];
        snprintf(log, sizeof(log), "Found %d task(s)", count);
        wchar_t wlog[256];
        MultiByteToWideChar(CP_UTF8, 0, log, -1, wlog, sizeof(wlog) / sizeof(wchar_t));
        add_log(wlog);
        
        /* 处理每个任务 */
        for (int i = 0; i < count; i++) {
            add_log(L"----------------------------------------");
            
            /* 显示Job ID */
            snprintf(log, sizeof(log), "Print Job ID: %s", tasks[i].job_id);
            MultiByteToWideChar(CP_UTF8, 0, log, -1, wlog, sizeof(wlog) / sizeof(wchar_t));
            add_log(wlog);
            
            /* 显示Task ID */
            snprintf(log, sizeof(log), "Task ID: %s", tasks[i].task_id);
            MultiByteToWideChar(CP_UTF8, 0, log, -1, wlog, sizeof(wlog) / sizeof(wchar_t));
            add_log(wlog);
            
            /* 显示File ID */
            snprintf(log, sizeof(log), "File ID: %s", tasks[i].file_id);
            MultiByteToWideChar(CP_UTF8, 0, log, -1, wlog, sizeof(wlog) / sizeof(wchar_t));
            add_log(wlog);
            
            /* 显示文件名 */
            snprintf(log, sizeof(log), "Filename: %s", tasks[i].filename);
            MultiByteToWideChar(CP_UTF8, 0, log, -1, wlog, sizeof(wlog) / sizeof(wchar_t));
            add_log(wlog);
            
            /* 下载文件到本地 */
            add_log(L"Downloading file...");
            char local_path[MAX_PATH];
            if (download_file_to_local(g_http_client, tasks[i].file_id, tasks[i].filename, local_path, sizeof(local_path)) == 0) {
                /* 本地路径 */
                snprintf(log, sizeof(log), "Saved to: %s", local_path);
                MultiByteToWideChar(CP_UTF8, 0, log, -1, wlog, sizeof(wlog) / sizeof(wchar_t));
                add_log(wlog);
                
                /* 打印文件 */
                add_log(L"Printing file...");
                if (print_file_to_default_printer(local_path) == 0) {
                    add_log(L"Print completed successfully");
                    
                    /* 通知服务器打印成功 */
                    add_log(L"Reporting task success to server...");
                    if (report_task_succeeded(g_http_client, tasks[i].task_id) == 0) {
                        add_log(L"Server notified successfully");
                    } else {
                        add_log(L"Failed to notify server");
                    }
                } else {
                    add_log(L"Print failed");
                }
            } else {
                add_log(L"Download failed");
            }
        }
        
        free(tasks);
    } else {
        add_log(L"Failed to fetch print jobs");
    }
}

/* ==================== 更新状态显示 ==================== */
/*
 * 更新主窗口的状态文本
 * 用于向用户显示当前程序状态
 */
void set_status(const wchar_t *status) {
    if (g_status_static) {
        SetWindowTextW(g_status_static, status);
    }
}

/* ==================== 添加日志 ==================== */
/*
 * 向日志列表添加一行
 * 自动滚动到最新
 */
void add_log(const wchar_t *msg) {
    if (g_log_static) {
        int index = SendMessageW(g_log_static, LB_ADDSTRING, 0, (LPARAM)msg);
        SendMessageW(g_log_static, LB_SETCURSEL, (WPARAM)index, 0);
    }
}
