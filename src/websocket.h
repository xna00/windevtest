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
    WebSocketMessageCallback on_message;  /* 消息回调函数 */
    int connected;                      /* 连接状态 */
} WebSocketClient;

/* ==================== 函数声明 ==================== */
/*
 * 初始化WebSocket客户端
 * @param cookie 认证Cookie
 * @return WebSocketClient指针
 */
WebSocketClient* ws_init(const char *cookie);

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

#endif
