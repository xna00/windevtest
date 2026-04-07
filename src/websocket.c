#define CURL_STATICLIB
#include "websocket.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t ws_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char **message = (char **)userp;
    
    char *ptr = realloc(*message, realsize + 1);
    if (ptr == NULL) return 0;
    
    *message = ptr;
    memcpy(*message, contents, realsize);
    (*message)[realsize] = 0;
    
    return realsize;
}

WebSocketClient* ws_init(const char *cookie) {
    WebSocketClient *ws = (WebSocketClient *)malloc(sizeof(WebSocketClient));
    if (!ws) return NULL;
    
    ws->curl = curl_easy_init();
    ws->cookie = cookie;
    ws->on_message = NULL;
    ws->connected = 0;
    
    return ws;
}

void ws_cleanup(WebSocketClient *ws) {
    if (ws) {
        if (ws->curl) curl_easy_cleanup(ws->curl);
        free(ws);
    }
}

int ws_connect(WebSocketClient *ws, const char *url) {
    if (!ws || !ws->curl) return -1;
    
    char *response = NULL;
    
    curl_easy_reset(ws->curl);
    curl_easy_setopt(ws->curl, CURLOPT_URL, url);
    curl_easy_setopt(ws->curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(ws->curl, CURLOPT_WRITEFUNCTION, ws_write_callback);
    curl_easy_setopt(ws->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(ws->curl, CURLOPT_TIMEOUT, 30L);
    
    if (ws->cookie) {
        curl_easy_setopt(ws->curl, CURLOPT_COOKIE, ws->cookie);
    }
    
    CURLcode res = curl_easy_perform(ws->curl);
    
    if (res == CURLE_OK) {
        long response_code;
        curl_easy_getinfo(ws->curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code == 101) {
            ws->connected = 1;
            return 0;
        }
    }
    
    if (response) free(response);
    return -1;
}

void ws_set_message_callback(WebSocketClient *ws, WebSocketMessageCallback callback) {
    if (ws) {
        ws->on_message = callback;
    }
}

int ws_send(WebSocketClient *ws, const char *message) {
    if (!ws || !ws->connected) return -1;
    
    size_t sent = 0;
    CURLcode res = curl_ws_send(ws->curl, message, strlen(message), &sent, 0, CURLWS_TEXT);
    
    return (res == CURLE_OK) ? 0 : -1;
}

int ws_receive(WebSocketClient *ws, char *buffer, size_t buf_size, size_t *recv_bytes) {
    if (!ws || !ws->connected) return -1;
    
    const struct curl_ws_frame *frame = NULL;
    CURLcode res = curl_ws_recv(ws->curl, buffer, buf_size, recv_bytes, &frame);
    
    if (res == CURLE_OK) {
        return 0;
    }
    
    return -1;
}