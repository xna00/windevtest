#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <curl/curl.h>

typedef void (*WebSocketMessageCallback)(const char *message);

typedef struct {
    CURL *curl;
    const char *cookie;
    WebSocketMessageCallback on_message;
    int connected;
} WebSocketClient;

WebSocketClient* ws_init(const char *cookie);
void ws_cleanup(WebSocketClient *ws);
int ws_connect(WebSocketClient *ws, const char *url);
void ws_set_message_callback(WebSocketClient *ws, WebSocketMessageCallback callback);
int ws_send(WebSocketClient *ws, const char *message);

#endif