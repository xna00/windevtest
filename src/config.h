#ifndef CONFIG_H
#define CONFIG_H

#define API_BASE_URL "http://192.168.1.100:8080"
#define API_USER_CURRENT API_BASE_URL "/api/user/currentUser"
#define API_LOGIN API_BASE_URL "/api/user/login"
#define API_WAITING_PRINTJOBS API_BASE_URL "/api/printjobs/waitingPrintjobs"
#define API_GET_FILE API_BASE_URL "/files/"
#define API_TASK_SUCCEED API_BASE_URL "/api/printJob/taskSucced"
#define API_WEBSOCKET_URL "ws://192.168.1.100:8080/ws/print"

#define WINDOW_CLASS_NAME "PrintDriverWindow"
#define WINDOW_TITLE "Print Driver"
#define TRAY_ICON_ID 1

#endif