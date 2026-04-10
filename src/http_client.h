/*
 * HTTP客户端头文件
 * 
 * 功能说明:
 * - 提供HTTP GET/POST请求封装
 * - 管理Cookie用于身份验证
 * - 支持JSON响应解析
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <curl/curl.h>
#include <json-c/json.h>

/* ==================== 数据结构 ==================== */
/*
 * HTTP客户端结构体
 * - cookie: 保存身份验证的token
 * - curl: libcurl句柄
 */
typedef struct {
    char *cookie;  /* 保存的Cookie信息 */
    CURL *curl;    /* libcurl句柄 */
} HttpClient;

/* ==================== 生命周期函数 ==================== */
/* 初始化HTTP客户端 */
HttpClient* http_client_init(void);

/* 清理HTTP客户端，释放资源 */
void http_client_cleanup(HttpClient *client);

/* ==================== HTTP请求函数 ==================== */
/*
 * 发送GET请求
 * @param client HTTP客户端
 * @param url 请求URL
 * @param response 响应内容输出
 * @param status_code HTTP状态码输出
 * @return 0成功，-1失败
 */
int http_get(HttpClient *client, const char *url, char **response, long *status_code);

/*
 * 发送POST请求（自动设置Content-Type为application/json）
 * 会从响应头中提取Set-Cookie
 */
int http_post(HttpClient *client, const char *url, const char *post_data, char **response, long *status_code);

/*
 * 发送POST请求（自定义Content-Type为application/json）
 * 不会自动处理Cookie
 */
int http_post_with_body(HttpClient *client, const char *url, const char *post_data, char **response, long *status_code);

/*
 * 发送POST请求，使用已保存的Cookie
 * Content-Type设置为text/plain;charset=UTF-8
 * 用于需要身份验证的API调用
 */
int http_post_with_client_cookie(HttpClient *client, const char *url, const char *post_data, char **response, long *status_code);

/*
 * 发送GET请求，带指定Cookie
 */
int http_get_with_cookie(HttpClient *client, const char *url, const char *cookie, char **response, long *status_code);

/*
 * 发送GET请求，带指定Cookie，返回二进制数据
 * @param client HTTP客户端
 * @param url 请求URL
 * @param cookie 认证Cookie
 * @param response 输出：响应内容
 * @param size 输出：响应大小（二进制数据可能包含\0）
 * @param status_code HTTP状态码
 * @return 0成功，-1失败
 */
int http_get_binary(HttpClient *client, const char *url, const char *cookie, char **response, size_t *size, long *status_code);

/* ==================== Cookie管理函数 ==================== */
/* 获取当前保存的Cookie */
char* http_client_get_cookie(HttpClient *client);

/* 设置Cookie */
void http_client_set_cookie(HttpClient *client, const char *cookie);

/* 将Cookie保存到文件（持久化） */
void http_client_save_cookie(HttpClient *client, const char *filepath);

/* 从文件加载Cookie */
void http_client_load_cookie(HttpClient *client, const char *filepath);

/* ==================== JSON解析函数 ==================== */
/*
 * 解析JSON字符串
 * @param json_str JSON字符串
 * @return json_object指针，失败返回NULL
 */
json_object* parse_json_response(const char *json_str);

#endif
