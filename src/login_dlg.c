#define CURL_STATICLIB
#include "login_dlg.h"
#include "http_client.h"
#include "config.h"
#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <winuser.h>

extern HttpClient *g_http_client;

INT_PTR CALLBACK LoginDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG:
            return TRUE;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                char username[256] = {0};
                char password[256] = {0};
                
                GetDlgItemTextA(hwnd, IDC_USERNAME, username, sizeof(username));
                GetDlgItemTextA(hwnd, IDC_PASSWORD, password, sizeof(password));
                
                if (strlen(username) == 0 || strlen(password) == 0) {
                    MessageBoxA(hwnd, "Please enter username and password", "Error", MB_OK | MB_ICONERROR);
                    return TRUE;
                }
                
                char json_body[512];
                snprintf(json_body, sizeof(json_body), 
                    "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
                
                char *response = NULL;
                long status_code = 0;
                
                int ret = http_post(g_http_client, API_LOGIN, json_body, &response, &status_code);
                
                if (ret == 0 && status_code == 200 && response) {
                    json_object *root = parse_json_response(response);
                    if (root) {
                        json_object *success_obj;
                        if (json_object_object_get_ex(root, "success", &success_obj)) {
                            int success = json_object_get_boolean(success_obj);
                            if (success) {
                                EndDialog(hwnd, IDOK);
                            } else {
                                json_object *msg_obj;
                                if (json_object_object_get_ex(root, "message", &msg_obj)) {
                                    MessageBoxA(hwnd, json_object_get_string(msg_obj), "Login Failed", MB_OK | MB_ICONERROR);
                                }
                            }
                            json_object_put(root);
                        } else {
                            json_object_put(root);
                        }
                    }
                    free(response);
                } else {
                    MessageBoxA(hwnd, "Login request failed", "Error", MB_OK | MB_ICONERROR);
                }
                
                return TRUE;
            }
            else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
                return TRUE;
            }
            break;
    }
    return FALSE;
}