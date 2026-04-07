#define CURL_STATICLIB
#include "http_client.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MemoryBuffer {
    char *data;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryBuffer *mem = (struct MemoryBuffer *)userp;
    
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (ptr == NULL) return 0;
    
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    
    return realsize;
}

static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    char **cookie_out = (char **)userdata;
    size_t buffer_size = size * nitems;
    
    if (strncmp(buffer, "Set-Cookie:", 11) == 0) {
        char *start = buffer + 11;
        while (*start == ' ') start++;
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

HttpClient* http_client_init(void) {
    HttpClient *client = (HttpClient *)malloc(sizeof(HttpClient));
    if (!client) return NULL;
    
    client->cookie = NULL;
    client->curl = curl_easy_init();
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    return client;
}

void http_client_cleanup(HttpClient *client) {
    if (client) {
        if (client->curl) curl_easy_cleanup(client->curl);
        if (client->cookie) free(client->cookie);
        free(client);
    }
}

int http_get(HttpClient *client, const char *url, char **response, long *status_code) {
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
    
    CURLcode res = curl_easy_perform(client->curl);
    
    if (res != CURLE_OK) {
        free(chunk.data);
        return -1;
    }
    
    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, status_code);
    *response = chunk.data;
    
    return 0;
}

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

char* http_client_get_cookie(HttpClient *client) {
    return client ? client->cookie : NULL;
}

void http_client_set_cookie(HttpClient *client, const char *cookie) {
    if (client && cookie) {
        if (client->cookie) free(client->cookie);
        client->cookie = _strdup(cookie);
    }
}

json_object* parse_json_response(const char *json_str) {
    if (!json_str) return NULL;
    return json_tokener_parse(json_str);
}