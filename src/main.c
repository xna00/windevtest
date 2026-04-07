#define CURL_STATICLIB
#include <windows.h>
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

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_SHOW 1
#define ID_TRAY_EXIT 2

static HINSTANCE g_hInst;
static HWND g_hwnd;
HttpClient *g_http_client = NULL;
static WebSocketClient *g_ws_client = NULL;
static char g_username[256] = {0};
static NOTIFYICONDATA g_nid;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void show_login_dialog(void);
void check_current_user(void);
void connect_websocket(void);
void on_websocket_message(const char *message);
void handle_print_job(void);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;
    
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);
    
    g_hwnd = CreateWindowA(WINDOW_CLASS_NAME, WINDOW_TITLE, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInstance, NULL);
    
    if (!g_hwnd) return 0;
    
    g_http_client = http_client_init();
    
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = TRAY_ICON_ID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(g_nid.szTip, "Print Driver");
    Shell_NotifyIcon(NIM_ADD, &g_nid);
    
    ShowWindow(g_hwnd, SW_HIDE);
    
    check_current_user();
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAYICON:
            if (lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONDOWN) {
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            }
            break;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_TRAY_SHOW) {
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            }
            else if (LOWORD(wParam) == ID_TRAY_EXIT) {
                Shell_NotifyIcon(NIM_DELETE, &g_nid);
                PostQuitMessage(0);
            }
            break;
            
        case WM_DESTROY:
            if (g_http_client) http_client_cleanup(g_http_client);
            if (g_ws_client) ws_cleanup(g_ws_client);
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void check_current_user(void) {
    char *response = NULL;
    long status_code = 0;
    
    int ret = http_get(g_http_client, API_USER_CURRENT, &response, &status_code);
    
    if (ret == 0 && status_code == 401) {
        show_login_dialog();
    }
    else if (ret == 0 && status_code == 200 && response) {
        json_object *root = parse_json_response(response);
        if (root) {
            json_object *username_obj;
            if (json_object_object_get_ex(root, "username", &username_obj)) {
                strncpy(g_username, json_object_get_string(username_obj), sizeof(g_username) - 1);
                
                char welcome[512];
                snprintf(welcome, sizeof(welcome), "Welcome: %s", g_username);
                MessageBoxA(NULL, welcome, "Logged In", MB_OK | MB_ICONINFORMATION);
            }
            json_object_put(root);
        }
        free(response);
        
        connect_websocket();
    }
}

void show_login_dialog(void) {
    DialogBoxParamA(g_hInst, MAKEINTRESOURCEA(IDC_LOGIN_DLG), g_hwnd, LoginDialogProc, 0);
    
    check_current_user();
}

void connect_websocket(void) {
    const char *cookie = http_client_get_cookie(g_http_client);
    g_ws_client = ws_init(cookie);
    
    if (ws_connect(g_ws_client, API_WEBSOCKET_URL) == 0) {
        printf("WebSocket connected\n");
        
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
    }
    else {
        printf("WebSocket connection failed\n");
    }
}

void on_websocket_message(const char *message) {
    printf("WebSocket message: %s\n", message);
    handle_print_job();
}

void handle_print_job(void) {
    PrintTaskInfo *tasks = NULL;
    int count = 0;
    
    if (get_waiting_print_jobs(g_http_client, &tasks, &count) == 0 && count > 0) {
        for (int i = 0; i < count; i++) {
            printf("Processing task: %s\n", tasks[i].task_id);
            
            if (download_and_print_file(g_http_client, tasks[i].file_id) == 0) {
                report_task_succeeded(g_http_client, tasks[i].task_id);
            }
        }
        
        free(tasks);
    }
}