/*
 * PrintDriver - 主程序入口
 * 
 * 功能说明:
 * 1. 系统托盘运行，最小化到托盘
 * 2. 登录验证，与服务器通信
 * 3. WebSocket实时接收打印任务
 * 4. 自动下载并打印文件
 * 5. 打印机设置管理
 */

#define CURL_STATICLIB

#include <windows.h>
#include <process.h>
#include <winuser.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "http_client.h"
#include "login_dlg.h"
#include "print_job.h"
#include "print_core.h"
#include "file_downloader.h"
#include "device_id.h"
#include "websocket.h"
#include "ui.h"
#include "updater.h"
#include "resource.h"

#include <json-c/json.h>

#pragma comment(lib, "winspool.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_SHOW 1
#define ID_TRAY_EXIT 2
#define DEBUG_MODE 1
#define WM_REFRESH_PRINTER_LIST (WM_USER + 100)

static HINSTANCE g_hInst;
static HWND g_hwnd;
static HWND g_status_static;
HttpClient *g_http_client = NULL;
static WebSocketClient *g_ws_client = NULL;
static char g_username[256] = {0};
char g_computer_id[256] = {0};
char g_computer_name[256] = {0};
static NOTIFYICONDATAW g_nid;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void check_current_user_thread(void *arg);
void check_current_user(void);
void show_login_dialog(void);
void connect_websocket(void);
void on_websocket_message(const char *message);
void handle_print_job(void);
void register_computer(void);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;
    
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"PrintDriverWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    
    g_hwnd = CreateWindowW(
        L"PrintDriverWindow",
        L"Print Driver",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 620,
        NULL, NULL,
        hInstance, NULL
    );
    
    if (!g_hwnd) return 0;
    
    g_http_client = http_client_init();
    http_client_load_cookie(g_http_client, COOKIE_FILE);
    
    start_update_check(g_http_client, g_hwnd);
    
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, _countof(g_nid.szTip), L"Print Driver");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    
    ShowWindow(g_hwnd, SW_SHOW);
    
    _beginthread(check_current_user_thread, 0, NULL);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            g_status_static = NULL;
            init_ui_controls(hwnd, g_hInst);
            init_printer_tab();
            
            if (enum_local_printers(&g_local_printers) != 0) {
                add_log(L"枚举本地打印机失败");
            }
            
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
            
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_TRAY_SHOW) {
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            }
            else if (LOWORD(wParam) == ID_TRAY_EXIT) {
                Shell_NotifyIconW(NIM_DELETE, &g_nid);
                PostQuitMessage(0);
            }
            else {
                handle_button_click(hwnd, LOWORD(wParam));
            }
            break;
            
        case WM_NOTIFY:
            if (((LPNMHDR)lParam)->hwndFrom == g_tab_ctrl) {
                if (((LPNMHDR)lParam)->code == TCN_SELCHANGE) {
                    on_tab_changed(hwnd);
                }
            }
            break;
            
        case WM_REFRESH_PRINTER_LIST:
            refresh_printer_list();
            return 0;
            
        case WM_SIZE:
            return 0;
            
        case WM_ERASEBKGND:
            return 1;
            
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONDOWN) {
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
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            }
            break;
            
        case WM_CLOSE:
#if DEBUG_MODE
            if (g_http_client) http_client_cleanup(g_http_client);
            if (g_ws_client) ws_cleanup(g_ws_client);
            PostQuitMessage(0);
#else
            ShowWindow(hwnd, SW_HIDE);
#endif
            return 0;
            
        case WM_DESTROY:
            if (g_http_client) http_client_cleanup(g_http_client);
            if (g_ws_client) ws_cleanup(g_ws_client);
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void check_current_user_thread(void *arg) {
    Sleep(100);
    check_current_user();
    _endthread();
}

void check_current_user(void) {
    char *response = NULL;
    long status_code = 0;
    
    add_log(L"检查用户状态...");
    
    int ret = http_post_with_client_cookie(g_http_client, API_USER_CURRENT, "[]", &response, &status_code);
    
    if (ret != 0) {
        add_log(L"连接失败");
        return;
    }
    
    if (status_code == 401) {
        add_log(L"未登录");
        show_login_dialog();
        return;
    }
    
    if (status_code == 200 && response) {
        json_object *root = parse_json_response(response);
        if (root) {
            json_object *username_obj;
            if (json_object_object_get_ex(root, "username", &username_obj)) {
                strncpy_s(g_username, sizeof(g_username), json_object_get_string(username_obj), _TRUNCATE);
                
                wchar_t welcome[256];
                swprintf(welcome, 256, L"已登录: %S", g_username);
                add_log(welcome);
            }
            json_object_put(root);
        }
        free(response);
        
        if (get_device_id(g_computer_id, sizeof(g_computer_id)) != 0) {
            add_log(L"获取设备ID失败");
        } else {
            wchar_t did[256];
            swprintf(did, 256, L"设备ID: %S", g_computer_id);
            add_log(did);
            
            register_computer();
        }
        
        connect_websocket();
    } else {
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

void show_login_dialog(void) {
    DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDC_LOGIN_DLG), g_hwnd, LoginDialogProc, 0);
    
    LoginResult *result = get_login_result();
    if (result->success) {
        strncpy_s(g_username, sizeof(g_username), result->username, _TRUNCATE);
        
        http_client_save_cookie(g_http_client, COOKIE_FILE);
        
        if (get_device_id(g_computer_id, sizeof(g_computer_id)) != 0) {
            add_log(L"获取设备ID失败");
        } else {
            wchar_t did[256];
            swprintf(did, 256, L"设备ID: %S", g_computer_id);
            add_log(did);
            
            register_computer();
        }
        
        connect_websocket();
    }
}

void register_computer(void) {
    ComputerInfo info = {0};
    add_log(L"检查计算机注册状态...");
    int ret = get_computer_info(g_http_client, g_computer_id, &info);
    
    wchar_t debug[256];
    swprintf(debug, 256, L"get_computer_info返回: %d", ret);
    add_log(debug);
    
    if (ret == -2) {
        add_log(L"计算机未找到，正在注册...");
        
        wchar_t computer_name[256] = {0};
        DWORD size = sizeof(computer_name) / sizeof(wchar_t);
        if (!GetComputerNameW(computer_name, &size)) {
            wcscpy_s(computer_name, _countof(computer_name), L"Unknown");
        }
        
        wchar_t name_log[256];
        swprintf(name_log, 256, L"计算机名称: %s", computer_name);
        add_log(name_log);
        
        char computer_name_utf8[256] = {0};
        WideCharToMultiByte(CP_UTF8, 0, computer_name, -1, computer_name_utf8, 256, NULL, NULL);
        
        if (add_computer(g_http_client, g_computer_id, computer_name_utf8) == 0) {
            add_log(L"计算机注册成功");
            
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
        add_log(L"计算机已注册");
        free_computer_info(&info);
    } else {
        add_log(L"检查计算机信息失败");
    }
}

void connect_websocket(void) {
    add_log(L"正在连接服务器...");
    
    const char *cookie = http_client_get_cookie(g_http_client);
    g_ws_client = ws_init(cookie, g_computer_id);
    
    if (!g_ws_client) {
        add_log(L"初始化WebSocket客户端失败");
        return;
    }
    
    while (g_ws_client->should_reconnect) {
        if (g_ws_client->reconnect_attempts >= WS_MAX_RECONNECT_ATTEMPTS) {
            add_log(L"已达到最大重连次数，停止重连");
            break;
        }
        
        if (g_ws_client->reconnect_attempts > 0) {
            wchar_t log[128];
            swprintf(log, 128, L"第 %d 次重连，等待 %d 秒...", 
                     g_ws_client->reconnect_attempts, WS_RECONNECT_DELAY / 1000);
            add_log(log);
            Sleep(WS_RECONNECT_DELAY);
        }
        
        if (ws_connect(g_ws_client, API_WEBSOCKET_URL) == 0) {
            add_log(L"WebSocket已连接");
            ws_reset_reconnect_attempts(g_ws_client);
            ws_update_message_time(g_ws_client);
            
            char buffer[4096];
            
            while (ws_is_connected(g_ws_client)) {
                if (ws_check_timeout(g_ws_client)) {
                    add_log(L"连接超时，准备重连...");
                    ws_disconnect(g_ws_client);
                    break;
                }
                
                size_t recv_bytes = 0;
                int ret = ws_receive(g_ws_client, buffer, sizeof(buffer), &recv_bytes);
                
                if (ret == 0 && recv_bytes > 0) {
                    buffer[recv_bytes] = '\0';
                    on_websocket_message(buffer);
                } else if (ret != 0) {
                    add_log(L"WebSocket连接已断开，准备重连...");
                    break;
                }
                
                Sleep(100);
            }
        } else {
            g_ws_client->reconnect_attempts++;
            wchar_t log[128];
            swprintf(log, 128, L"连接服务器失败 (尝试 %d/%d)", 
                     g_ws_client->reconnect_attempts, WS_MAX_RECONNECT_ATTEMPTS);
            add_log(log);
        }
    }
    
    ws_cleanup(g_ws_client);
    g_ws_client = NULL;
}

void on_websocket_message(const char *message) {
    if (!message || strlen(message) == 0) return;
    
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
    
    if (strcmp(msg_type, "check_jobs") == 0) {
        add_log(L"收到打印任务检查消息");
        handle_print_job();
    } else if (strcmp(msg_type, "heartbeat") == 0 || strcmp(msg_type, "ping") == 0) {
        add_log(L"收到心跳消息");
    } else {
        add_log(L"WebSocket收到消息:");
        wchar_t wmessage[512];
        MultiByteToWideChar(CP_UTF8, 0, message, -1, wmessage, 512);
        add_log(wmessage);
    }
    
    json_object_put(root);
}

void handle_print_job(void) {
    PrintTaskInfo *tasks = NULL;
    int count = 0;
    
    add_log(L"正在获取待打印任务...");
    
    if (get_waiting_print_jobs(g_http_client, g_computer_id, &tasks, &count) != 0) {
        add_log(L"获取打印任务失败");
        return;
    }
    
    if (count == 0) {
        add_log(L"没有待打印任务");
        return;
    }
    
    for (int i = 0; i < count; i++) {
        wchar_t log[512];
        swprintf(log, 512, L"正在打印任务 %d/%d", i + 1, count);
        add_log(log);
        
        char local_path[MAX_PATH];
        if (download_file_to_local(g_http_client, tasks[i].file_id, tasks[i].filename, local_path, sizeof(local_path)) != 0) {
            add_log(L"下载文件失败");
            report_task_succeeded(g_http_client, tasks[i].task_id);
            continue;
        }
        
        if (print_file_to_default_printer(local_path) != 0) {
            add_log(L"打印失败");
            report_task_succeeded(g_http_client, tasks[i].task_id);
            continue;
        }
        
        if (report_task_succeeded(g_http_client, tasks[i].task_id) == 0) {
            add_log(L"任务完成");
        } else {
            add_log(L"上报任务状态失败");
        }
    }
    
    free(tasks);
}
