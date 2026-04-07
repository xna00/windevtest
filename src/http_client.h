#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <curl/curl.h>
#include <json-c/json.h>

typedef struct {
    char *cookie;
    CURL *curl;
} HttpClient;

HttpClient* http_client_init(void);
void http_client_cleanup(HttpClient *client);

int http_get(HttpClient *client, const char *url, char **response, long *status_code);
int http_post(HttpClient *client, const char *url, const char *post_data, char **response, long *status_code);
int http_get_with_cookie(HttpClient *client, const char *url, const char *cookie, char **response, long *status_code);

char* http_client_get_cookie(HttpClient *client);
void http_client_set_cookie(HttpClient *client, const char *cookie);

json_object* parse_json_response(const char *json_str);

#endif