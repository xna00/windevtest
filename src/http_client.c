/*
 * HTTP客户端实现
 * 
 * 使用libcurl进行HTTP通信
 * 支持Cookie管理和响应缓存
 */

#define CURL_STATICLIB
#include "http_client.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 内部数据结构 ==================== */
/*
 * 内存缓冲区结构体
 * 用于接收HTTP响应数据
 */
struct MemoryBuffer {
    char *data;    /* 数据缓冲区 */
    size_t size;   /* 当前数据大小 */
};

/* ==================== 回调函数 ==================== */
/*
 * CURL写回调函数
 * 将接收到的数据追加到内存缓冲区
 */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryBuffer *mem = (struct MemoryBuffer *)userp;
    
    /* 扩展缓冲区 */
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (ptr == NULL) return 0;  /* 内存分配失败 */
    
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;  /* 添加字符串结束符 */
    
    return realsize;
}

/*
 * CURL响应头回调函数
 * 用于从Set-Cookie头中提取Cookie
 */
static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    char **cookie_out = (char **)userdata;
    size_t buffer_size = size * nitems;
    
    /* 检查是否是Set-Cookie头 */
    if (strncmp(buffer, "Set-Cookie:", 11) == 0) {
        char *start = buffer + 11;
        /* 跳过开头的空格 */
        while (*start == ' ') start++;
        /* 找到分号（Cookie值的结束位置） */
        char *end = strchr(start, ';');
        if (end) {
            size_t len = end - start;
            *cookie_out = malloc(len + 1);
            if (*cookie_out) {
                strncpy(*cookie_out, start, len);
                (*cookie_out)[len] = '\0';
            }
        }
    }
    return buffer_size;
}

/* ==================== 生命周期函数 ==================== */
/*
 * 初始化HTTP客户端
 * 创建curl句柄和Cookie存储
 */
HttpClient* http_client_init(void) {
    HttpClient *client = (HttpClient *)malloc(sizeof(HttpClient));
    if (!client) return NULL;
    
    client->cookie = NULL;
    client->curl = curl_easy_init();
    curl_global_init(CURL_GLOBAL_ALL);  /* 初始化curl库 */
    
    return client;
}

/*
 * 清理HTTP客户端
 * 释放curl句柄和Cookie
 */
void http_client_cleanup(HttpClient *client) {
    if (client) {
        if (client->curl) curl_easy_cleanup(client->curl);
        if (client->cookie) free(client->cookie);
        free(client);
    }
}

/* ==================== HTTP请求函数 ==================== */
/*
 * 发送GET请求
 * 简单的HTTP GET，不带Cookie
 */
int http_get(HttpClient *client, const char *url, char **response, long *status_code) {
    if (!client || !client->curl) return -1;
    
    struct MemoryBuffer chunk;
    chunk.data = malloc(1);
    chunk.size = 0;
    
    curl_easy_reset(client->curl);
    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1L);  /* 自动跟随重定向 */
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 30L);  /* 30秒超时 */
    
    CURLcode res = curl_easy_perform(client->curl);
    
    if (res != CURLE_OK) {
        free(chunk.data);
        return -1;
    }
    
    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, status_code);
    *response = chunk.data;
    
    return 0;
}

/*
 * 发送POST请求（application/json）
 * 会自动从响应头提取Set-Cookie
 */
int http_post(HttpClient *client, const char *url, const char *post_data, char **response, long *status_code) {
    if (!client || !client->curl) return -1;
    
    struct MemoryBuffer chunk;
    chunk.data = malloc(1);
    chunk.size = 0;
    
    char *cookie_header = NULL;
    
    curl_easy_reset(client->curl);
    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(client->curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(client->curl, CURLOPT_HEADERDATA, &cookie_header);
    curl_easy_setopt(client->curl, CURLOPT_POSTFIELDSIZE, strlen(post_data));
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(client->curl);
    curl_slist_free_all(headers);
    
    if (res == CURLE_OK) {
        curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, status_code);
        
        /* 保存从服务器收到的Cookie */
        if (cookie_header && !client->cookie) {
            client->cookie = cookie_header;
        } else if (cookie_header) {
            free(cookie_header);
        }
        
        *response = chunk.data;
        return 0;
    }
    
    free(chunk.data);
    if (cookie_header) free(cookie_header);
    return -1;
}

/*
 * 发送POST请求（application/json）
 * 不自动处理Cookie
 */
int http_post_with_body(HttpClient *client, const char *url, const char *post_data, char **response, long *status_code) {
    if (!client || !client->curl) return -1;
    
    struct MemoryBuffer chunk;
    chunk.data = malloc(1);
    chunk.size = 0;
    
    curl_easy_reset(client->curl);
    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(client->curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(client->curl, CURLOPT_POSTFIELDSIZE, strlen(post_data));
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 30L);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(client->curl);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        free(chunk.data);
        return -1;
    }
    
    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, status_code);
    *response = chunk.data;
    
    return 0;
}

/*
 * 发送GET请求，带指定Cookie
 */
int http_get_with_cookie(HttpClient *client, const char *url, const char *cookie, char **response, long *status_code) {
    if (!client || !client->curl) return -1;
    
    struct MemoryBuffer chunk;
    chunk.data = malloc(1);
    chunk.size = 0;
    
    curl_easy_reset(client->curl);
    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 30L);
    
    /* 设置Cookie */
    if (cookie) {
        curl_easy_setopt(client->curl, CURLOPT_COOKIE, cookie);
    }
    
    CURLcode res = curl_easy_perform(client->curl);
    
    if (res != CURLE_OK) {
        free(chunk.data);
        return -1;
    }
    
    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, status_code);
    *response = chunk.data;
    
    return 0;
}

/*
 * 发送GET请求，带指定Cookie，返回二进制数据
 * 响应大小保存在size中，不依赖\0终止符
 */
int http_get_binary(HttpClient *client, const char *url, const char *cookie, char **response, size_t *size, long *status_code) {
    if (!client || !client->curl) return -1;
    
    struct MemoryBuffer chunk;
    chunk.data = malloc(1);
    chunk.size = 0;
    
    curl_easy_reset(client->curl);
    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 30L);
    
    /* 设置Cookie */
    if (cookie) {
        curl_easy_setopt(client->curl, CURLOPT_COOKIE, cookie);
    }
    
    CURLcode res = curl_easy_perform(client->curl);
    
    if (res != CURLE_OK) {
        free(chunk.data);
        return -1;
    }
    
    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, status_code);
    *response = chunk.data;
    *size = chunk.size;  /* 返回实际大小 */
    
    return 0;
}

/* ==================== Cookie管理函数 ==================== */
/*
 * 获取当前保存的Cookie
 */
char* http_client_get_cookie(HttpClient *client) {
    return client ? client->cookie : NULL;
}

/*
 * 设置Cookie（会释放旧的Cookie）
 */
void http_client_set_cookie(HttpClient *client, const char *cookie) {
    if (client && cookie) {
        if (client->cookie) free(client->cookie);
        client->cookie = _strdup(cookie);
    }
}

/*
 * 将Cookie保存到文件
 * 用于程序重启后恢复登录状态
 */
void http_client_save_cookie(HttpClient *client, const char *filepath) {
    if (client && client->cookie && filepath) {
        FILE *f = fopen(filepath, "w");
        if (f) {
            fprintf(f, "%s", client->cookie);
            fclose(f);
        }
    }
}

/*
 * 从文件加载Cookie
 * 程序启动时调用，恢复之前的登录状态
 */
void http_client_load_cookie(HttpClient *client, const char *filepath) {
    if (client && filepath) {
        FILE *f = fopen(filepath, "r");
        if (f) {
            char buffer[512];
            if (fgets(buffer, sizeof(buffer), f)) {
                /* 移除换行符 */
                size_t len = strlen(buffer);
                if (len > 0 && buffer[len-1] == '\n') {
                    buffer[len-1] = '\0';
                }
                http_client_set_cookie(client, buffer);
            }
            fclose(f);
        }
    }
}

/*
 * 发送POST请求，使用已保存的Cookie
 * Content-Type: text/plain;charset=UTF-8
 * 用于需要身份验证的API调用（如currentUser）
 */
int http_post_with_client_cookie(HttpClient *client, const char *url, const char *post_data, char **response, long *status_code) {
    if (!client || !client->curl) return -1;
    
    struct MemoryBuffer chunk;
    chunk.data = malloc(1);
    chunk.size = 0;
    
    curl_easy_reset(client->curl);
    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(client->curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(client->curl, CURLOPT_POSTFIELDSIZE, strlen(post_data));
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 30L);
    
    /* 使用已保存的Cookie进行身份验证 */
    if (client->cookie) {
        curl_easy_setopt(client->curl, CURLOPT_COOKIE, client->cookie);
    }
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: text/plain;charset=UTF-8");
    curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(client->curl);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        free(chunk.data);
        return -1;
    }
    
    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, status_code);
    *response = chunk.data;
    
    return 0;
}

/* ==================== JSON解析函数 ==================== */
/*
 * 解析JSON字符串
 * 使用json-c库解析
 */
json_object* parse_json_response(const char *json_str) {
    if (!json_str) return NULL;
    return json_tokener_parse(json_str);
}
