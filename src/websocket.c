/*
 * WebSocket客户端实现
 * 
 * 使用libcurl的WebSocket支持（CURLOPT_CONNECT_ONLY）
 * 实现实时消息推送功能
 */

#define CURL_STATICLIB
#include "websocket.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * WebSocket写回调
 * 用于接收WebSocket响应
 */
static size_t ws_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char **message = (char **)userp;
    
    /* 动态扩展缓冲区 */
    char *ptr = realloc(*message, realsize + 1);
    if (ptr == NULL) return 0;
    
    *message = ptr;
    memcpy(*message, contents, realsize);
    (*message)[realsize] = 0;
    
    return realsize;
}

/*
 * 初始化WebSocket客户端
 */
WebSocketClient* ws_init(const char *cookie, const char *computer_id) {
    WebSocketClient *ws = (WebSocketClient *)malloc(sizeof(WebSocketClient));
    if (!ws) return NULL;
    
    ws->curl = curl_easy_init();
    ws->cookie = cookie;
    ws->computer_id = computer_id;
    ws->on_message = NULL;
    ws->connected = 0;
    ws->reconnect_attempts = 0;
    ws->should_reconnect = 1;
    
    return ws;
}

/*
 * 清理WebSocket客户端
 */
void ws_cleanup(WebSocketClient *ws) {
    if (ws) {
        if (ws->curl) curl_easy_cleanup(ws->curl);
        free(ws);
    }
}

/*
 * 连接到WebSocket服务器
 * 使用CURLOPT_CONNECT_ONLY模式建立WebSocket连接
 * 在header中添加计算机ID
 */
int ws_connect(WebSocketClient *ws, const char *url) {
    if (!ws || !ws->curl) return -1;
    
    char *response = NULL;
    struct curl_slist *headers = NULL;
    char computer_id_header[512];
    
    curl_easy_reset(ws->curl);
    curl_easy_setopt(ws->curl, CURLOPT_URL, url);
    curl_easy_setopt(ws->curl, CURLOPT_CONNECT_ONLY, 2L);  /* WebSocket模式 */
    curl_easy_setopt(ws->curl, CURLOPT_WRITEFUNCTION, ws_write_callback);
    curl_easy_setopt(ws->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(ws->curl, CURLOPT_TIMEOUT, 30L);
    
    /* 设置认证Cookie */
    if (ws->cookie) {
        curl_easy_setopt(ws->curl, CURLOPT_COOKIE, ws->cookie);
    }
    
    /* 添加计算机ID到header
     * curl_slist_append - 添加字符串到链表
     * 参数1: 链表头，NULL表示创建新链表
     * 参数2: 要添加的字符串
     * 返回值: 新的链表头指针
     */
    if (ws->computer_id) {
        snprintf(computer_id_header, sizeof(computer_id_header), "X-Computer-ID: %s", ws->computer_id);
        headers = curl_slist_append(headers, computer_id_header);
        curl_easy_setopt(ws->curl, CURLOPT_HTTPHEADER, headers);
    }
    
    CURLcode res = curl_easy_perform(ws->curl);
    
    /* 释放header链表
     * curl_slist_free_all - 释放整个链表
     * 参数: 链表头指针
     */
    if (headers) {
        curl_slist_free_all(headers);
    }
    
    if (res == CURLE_OK) {
        long response_code;
        curl_easy_getinfo(ws->curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        /* 101表示WebSocket协议升级成功 */
        if (response_code == 101) {
            ws->connected = 1;
            return 0;
        }
    }
    
    if (response) free(response);
    return -1;
}

/*
 * 设置消息回调函数
 */
void ws_set_message_callback(WebSocketClient *ws, WebSocketMessageCallback callback) {
    if (ws) {
        ws->on_message = callback;
    }
}

/*
 * 发送WebSocket消息
 * 使用curl_ws_send发送数据帧
 */
int ws_send(WebSocketClient *ws, const char *message) {
    if (!ws || !ws->connected) return -1;
    
    size_t sent = 0;
    CURLcode res = curl_ws_send(ws->curl, message, strlen(message), &sent, 0, CURLWS_TEXT);
    
    return (res == CURLE_OK) ? 0 : -1;
}

/*
 * 接收WebSocket消息
 * 使用curl_ws_recv接收数据帧
 */
int ws_receive(WebSocketClient *ws, char *buffer, size_t buf_size, size_t *recv_bytes) {
    if (!ws || !ws->connected) return -1;
    
    const struct curl_ws_frame *frame = NULL;
    CURLcode res = curl_ws_recv(ws->curl, buffer, buf_size, recv_bytes, &frame);
    
    if (res == CURLE_OK) {
        return 0;
    }
    
    /* 接收失败，标记为断开连接 */
    ws->connected = 0;
    return -1;
}

/*
 * 断开WebSocket连接
 * 重置连接状态，准备重连
 */
void ws_disconnect(WebSocketClient *ws) {
    if (ws) {
        ws->connected = 0;
        if (ws->curl) {
            curl_easy_reset(ws->curl);
        }
    }
}

/*
 * 检查连接状态
 */
int ws_is_connected(WebSocketClient *ws) {
    return ws ? ws->connected : 0;
}

/*
 * 重置重连计数器
 * 连接成功后调用
 */
void ws_reset_reconnect_attempts(WebSocketClient *ws) {
    if (ws) {
        ws->reconnect_attempts = 0;
    }
}
