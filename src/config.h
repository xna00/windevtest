/*
 * 配置文件头文件
 * 
 * 功能说明:
 * - 定义服务器API地址
 * - 定义窗口和托盘图标常量
 * - 定义Cookie存储文件名
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ==================== 服务器配置 ==================== */
/*
 * 服务器基础URL
 * 所有API都基于此URL构建
 */
#define API_BASE_URL "http://superprint.xna00.top"

/*
 * 获取当前用户信息
 * 需要身份验证，发送POST请求，body为"[]"
 */
#define API_USER_CURRENT API_BASE_URL "/api/user/currentUser"

/*
 * 用户登录
 * 发送POST请求，body为JSON数组格式
 */
#define API_LOGIN API_BASE_URL "/api/auth/login"

/*
 * 获取打印任务列表（支持过滤）
 * 使用POST请求，body格式: [{"state": "waiting_print", "computerId": "xxx", "printerName": "xxx"}]
 * 参数都是可选的
 */
#define API_LIST_PRINTJOBS API_BASE_URL "/api/printJob/listPrintJobsWithTasks"

/*
 * 下载打印文件（普通文件）
 * 使用GET请求，参数格式: /api/files/getFile?data=["fileId"]
 */
#define API_GET_FILE API_BASE_URL "/api/files/getFile"

/*
 * 下载PS打印文件
 * 使用GET请求，参数格式: /api/files/getPsFile?data=["fileId"]
 */
#define API_GET_PS_FILE API_BASE_URL "/api/files/getPsFile"

/*
 * 报告打印任务成功
 * 使用POST请求，body格式: [taskId]
 */
#define API_TASK_SUCCEED API_BASE_URL "/api/printJob/taskSucced"

/* ==================== 计算机接口 ==================== */
/*
 * 获取计算机信息
 * 使用POST请求，body格式: [computerId]
 */
#define API_COMPUTER_INFO API_BASE_URL "/api/computer/computerInfo"

/*
 * 设置计算机名称
 * 使用POST请求，body格式: [computerId, newName]
 */
#define API_SET_COMPUTER_NAME API_BASE_URL "/api/computer/setComputerName"

/*
 * 添加计算机
 * 使用POST请求，body格式: [computerId, computerName]
 */
#define API_ADD_COMPUTER API_BASE_URL "/api/computer/addComputer"

/*
 * 添加打印机到计算机
 * 使用POST请求，body格式: [computerId, printerName]
 */
#define API_ADD_PRINTER API_BASE_URL "/api/computer/addComputerPrinter"

/*
 * 从计算机删除打印机
 * 使用POST请求，body格式: [computerId, printerName]
 */
#define API_REMOVE_PRINTER API_BASE_URL "/api/computer/removeComputerPrinter"

/*
 * WebSocket服务器地址
 * 用于实时接收打印任务通知
 */
#define API_WEBSOCKET_URL "ws://superprint.xna00.top/ws/print"

/* ==================== 窗口配置 ==================== */
/* 窗口类名 */
#define WINDOW_CLASS_NAME "PrintDriverWindow"

/* 窗口标题 */
#define WINDOW_TITLE "Print Driver"

/* 托盘图标ID */
#define TRAY_ICON_ID 1

/* ==================== Cookie配置 ==================== */
/*
 * Cookie文件路径
 * 用于持久化保存登录状态
 */
#define COOKIE_FILE "cookie.txt"

/* ==================== 下载配置 ==================== */
/*
 * 打印文件下载目录
 * 用于保存从服务器下载的打印文件
 */
#define DOWNLOAD_FOLDER "downloads"

#endif
