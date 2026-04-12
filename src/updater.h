#ifndef UPDATER_H
#define UPDATER_H

#include <windows.h>
#include "http_client.h"
#include "version.h"

#define CURRENT_VERSION PROJECT_VERSION
#define UPDATE_CHECK_URL "https://superprint6.xna00.top/api/version/check"

typedef struct _VersionInfo {
    char download_url[512];
    char release_notes[1024];
} VersionInfo;

int check_for_update(HttpClient *client, VersionInfo *info);
int download_update(HttpClient *client, const char *url, const wchar_t *save_path);
int start_update_check(HttpClient *client, HWND hwnd);

#endif
