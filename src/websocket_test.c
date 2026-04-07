#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#define sleep(s) Sleep((DWORD)(s * 1000))
#else
#include <unistd.h>
#endif

#include <curl/curl.h>

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    printf("Received: %.*s\n", (int)(size * nmemb), ptr);
    return size * nmemb;
}

int main(int argc, const char *argv[])
{
    CURL *curl;
    CURLcode result;

    result = curl_global_init(CURL_GLOBAL_ALL);
    if(result != CURLE_OK) {
        fprintf(stderr, "curl_global_init() failed\n");
        return 1;
    }

    curl = curl_easy_init();
    if(curl) {
        if(argc == 2)
            curl_easy_setopt(curl, CURLOPT_URL, argv[1]);
        else
            curl_easy_setopt(curl, CURLOPT_URL, "wss://example.com");

        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

        result = curl_easy_perform(curl);
        if(result != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(result));
        } else {
            printf("Connected to WebSocket server\n");

            const char *msg = "Hello, WebSocket!";
            size_t sent = 0;
            result = curl_ws_send(curl, msg, strlen(msg), &sent, 0, CURLWS_TEXT);
            if(result == CURLE_OK) {
                printf("Sent: %s (%zu bytes)\n", msg, sent);
                sleep(1);
            } else {
                fprintf(stderr, "curl_ws_send() failed: %s\n", curl_easy_strerror(result));
            }

            char buffer[4096];
            size_t recv_bytes = 0;
            const struct curl_ws_frame *frame = NULL;

            result = curl_ws_recv(curl, buffer, sizeof(buffer), &recv_bytes, &frame);
            if(result == CURLE_OK) {
                printf("Received: %.*s (%zu bytes, flags: %d)\n", 
                       (int)recv_bytes, buffer, recv_bytes, frame->flags);
            } else {
                fprintf(stderr, "curl_ws_recv() failed: %s\n", curl_easy_strerror(result));
            }
        }

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return 0;
}