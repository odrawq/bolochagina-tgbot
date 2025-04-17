#ifndef REQUESTS_H
    #define REQUESTS_H

    #include <stdint.h>

    #include <cjson/cJSON.h>

    #include "config.h"

    #define BOT_API_URL "https://api.telegram.org/bot" BOT_TOKEN

    #define MAX_URL_SIZE        512
    #define MAX_POSTFIELDS_SIZE 8192

    #define MAX_CONNECT_TIMEOUT  15
    #define MAX_RESPONSE_TIMEOUT 30

    #define MAX_REQUEST_RETRIES 3

    void init_requests_module(void);

    cJSON *get_updates(const int_fast32_t update_id);
    void leave_chat(const int_fast64_t chat_id);
    void send_message_with_keyboard(const int_fast64_t chat_id, const char *message, const char *keyboard);

#endif
