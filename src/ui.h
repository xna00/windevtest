#ifndef UI_H
#define UI_H

#include <windows.h>
#include "printer_manager.h"
#include "computer_manager.h"

#define ID_BTN_SAVE_NAME 100
#define ID_BTN_ENABLE_PRINTER 101
#define ID_BTN_DISABLE_PRINTER 102

extern HWND g_log_static;
extern HWND g_tab_ctrl;
extern HWND g_printer_name_edit;
extern HWND g_printer_list_view;
extern HWND g_btn_save_name;
extern HWND g_btn_enable;
extern HWND g_btn_disable;
extern HWND g_static_computer_name;
extern HWND g_static_printer_list;

extern char g_computer_id[256];
extern PrinterList g_local_printers;
extern ComputerInfo g_computer_info;

void init_ui_controls(HWND hwnd, HINSTANCE hInst);
void on_tab_changed(HWND hwnd);
void add_log(const wchar_t *msg);
void init_printer_tab(void);
void refresh_printer_list(void);
void on_save_computer_name(void);
void on_enable_printer(void);
void on_disable_printer(void);
void refresh_printer_list_thread(void *arg);
void handle_button_click(HWND hwnd, int button_id);

#endif