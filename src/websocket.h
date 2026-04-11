/*
 * WebSocket客户端头文件
 * 
 * 功能说明:
 * - 使用libcurl的WebSocket支持
 * - 与服务器建立WebSocket连接
 * - 发送和接收WebSocket消息
 */

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <curl/curl.h>

/* ==================== 配置常量 ==================== */
#define WS_RECONNECT_DELAY 5000      /* 重连延迟（毫秒） */
#define WS_MAX_RECONNECT_ATTEMPTS 10 /* 最大重连尝试次数 */
#define WS_TIMEOUT 60000             /* 连接超时（毫秒）- 超过此时间没收到消息则认为断开 */

/* ==================== 类型定义 ==================== */
/*
 * WebSocket消息回调函数类型
 * 当收到消息时调用此函数
 */
typedef void (*WebSocketMessageCallback)(const char *message);

/*
 * WebSocket客户端结构体
 */
typedef struct {
    CURL *curl;                         /* libcurl句柄 */
    const char *cookie;                 /* 认证Cookie */
    const char *computer_id;            /* 计算机ID */
    WebSocketMessageCallback on_message;  /* 消息回调函数 */
    int connected;                      /* 连接状态 */
    int reconnect_attempts;             /* 当前重连尝试次数 */
    int should_reconnect;               /* 是否应该重连 */
    DWORD last_message_time;            /* 上次收到消息的时间 */
} WebSocketClient;

/* ==================== 函数声明 ==================== */
/*
 * 初始化WebSocket客户端
 * @param cookie 认证Cookie
 * @param computer_id 计算机ID
 * @return WebSocketClient指针
 */
WebSocketClient* ws_init(const char *cookie, const char *computer_id);

/*
 * 清理WebSocket客户端
 * @param ws WebSocket客户端指针
 */
void ws_cleanup(WebSocketClient *ws);

/*
 * 连接到WebSocket服务器
 * @param ws WebSocket客户端
 * @param url WebSocket URL
 * @return 0成功，-1失败
 */
int ws_connect(WebSocketClient *ws, const char *url);

/*
 * 设置消息回调函数
 */
void ws_set_message_callback(WebSocketClient *ws, WebSocketMessageCallback callback);

/*
 * 发送WebSocket消息
 * @return 0成功，-1失败
 */
int ws_send(WebSocketClient *ws, const char *message);

/*
 * 接收WebSocket消息（阻塞）
 * @param buffer 接收缓冲区
 * @param buf_size 缓冲区大小
 * @param recv_bytes 实际接收字节数
 * @return 0成功，-1失败
 */
int ws_receive(WebSocketClient *ws, char *buffer, size_t buf_size, size_t *recv_bytes);

/*
 * 断开WebSocket连接
 * @param ws WebSocket客户端
 */
void ws_disconnect(WebSocketClient *ws);

/*
 * 检查连接状态
 * @return 1已连接，0未连接
 */
int ws_is_connected(WebSocketClient *ws);

/*
 * 重置重连计数器
 */
void ws_reset_reconnect_attempts(WebSocketClient *ws);

/*
 * 检查连接超时
 * @return 1超时，0正常
 */
int ws_check_timeout(WebSocketClient *ws);

/*
 * 更新消息时间
 */
void ws_update_message_time(WebSocketClient *ws);

#endif
