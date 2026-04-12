/*
 * 自动更新模块
 * 
 * 功能说明:
 * - 检查服务器是否有新版本
 * - 后台静默下载更新
 * - 下载安装程序进行更新
 */

#include "updater.h"
#include "config.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <windows.h>
#include <shellapi.h>
#include <process.h>

extern void add_log(const wchar_t *msg);

int check_for_update(HttpClient *client, VersionInfo *info) {
    if (!client || !info) return -1;
    
    memset(info, 0, sizeof(VersionInfo));
    
    char *response = NULL;
    long status_code = 0;
    
    char body[256];
    snprintf(body, sizeof(body), "[\"%s\"]", CURRENT_VERSION);
    
    int ret = http_post_with_client_cookie(client, UPDATE_CHECK_URL, body, &response, &status_code);
    
    if (ret != 0 || status_code != 200) {
        if (response) free(response);
        return -1;
    }
    
    json_object *root = json_tokener_parse(response);
    free(response);
    
    if (!root) return -1;
    
    json_object *url_obj, *msg_obj;
    
    if (json_object_object_get_ex(root, "downloadUrl", &url_obj)) {
        const char *url = json_object_get_string(url_obj);
        if (url && strlen(url) > 0) {
            strncpy_s(info->download_url, sizeof(info->download_url), url, _TRUNCATE);
        }
    }
    
    if (json_object_object_get_ex(root, "message", &msg_obj)) {
        const char *msg = json_object_get_string(msg_obj);
        if (msg) {
            strncpy_s(info->release_notes, sizeof(info->release_notes), msg, _TRUNCATE);
        }
    }
    
    json_object_put(root);
    
    if (info->download_url[0] != '\0') {
        return 1;
    }
    
    return 0;
}

static size_t write_file_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

int download_update(HttpClient *client, const char *url, const wchar_t *save_path) {
    if (!client || !url || !save_path) return -1;
    
    FILE *fp = NULL;
    _wfopen_s(&fp, save_path, L"wb");
    if (!fp) {
        return -1;
    }
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        return -1;
    }
    
    struct curl_slist *headers = NULL;
    const char *cookie = http_client_get_cookie(client);
    if (cookie && strlen(cookie) > 0) {
        char cookie_header[1024];
        snprintf(cookie_header, sizeof(cookie_header), "Cookie: %s", cookie);
        headers = curl_slist_append(headers, cookie_header);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    fclose(fp);
    
    if (res != CURLE_OK) {
        DeleteFileW(save_path);
        return -1;
    }
    
    return 0;
}

typedef struct {
    HttpClient *client;
    HWND hwnd;
} UpdateCheckParams;

void check_update_background(void *arg) {
    UpdateCheckParams *params = (UpdateCheckParams *)arg;
    if (!params) {
        _endthread();
        return;
    }
    
    HttpClient *client = params->client;
    HWND hwnd = params->hwnd;
    free(params);
    
    Sleep(3000);
    
    VersionInfo info = {0};
    int result = check_for_update(client, &info);
    
    if (result == 1 && info.download_url[0] != '\0') {
        if (info.release_notes[0] != '\0') {
            wchar_t wmsg[512];
            MultiByteToWideChar(CP_UTF8, 0, info.release_notes, -1, wmsg, 512);
            add_log(wmsg);
        }
        add_log(L"正在后台下载更新...");
        
        wchar_t temp_path[MAX_PATH];
        GetTempPathW(MAX_PATH, temp_path);
        wcscat_s(temp_path, MAX_PATH, L"PrintDriver-Setup.exe");
        
        if (download_update(client, info.download_url, temp_path) == 0) {
            add_log(L"更新下载完成，正在安装...");
            
            SHELLEXECUTEINFOW sei = {0};
            sei.cbSize = sizeof(sei);
            sei.lpFile = temp_path;
            sei.lpParameters = L"/VERYSILENT /SUPPRESSMSGBOXES /NORESTART";
            sei.nShow = SW_HIDE;
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            
            if (ShellExecuteExW(&sei)) {
                add_log(L"更新安装已启动，程序将退出");
                Sleep(2000);
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            } else {
                add_log(L"启动更新程序失败");
            }
        } else {
            add_log(L"更新下载失败");
        }
    }
    
    _endthread();
}

int start_update_check(HttpClient *client, HWND hwnd) {
    UpdateCheckParams *params = malloc(sizeof(UpdateCheckParams));
    if (!params) return -1;
    
    params->client = client;
    params->hwnd = hwnd;
    
    _beginthread(check_update_background, 0, params);
    return 0;
}
