#ifndef LOGIN_DLG_H
#define LOGIN_DLG_H

#include <windows.h>

#define IDC_USERNAME 100
#define IDC_PASSWORD 101

typedef struct {
    char username[256];
    char password[256];
    int success;
} LoginResult;

INT_PTR CALLBACK LoginDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif