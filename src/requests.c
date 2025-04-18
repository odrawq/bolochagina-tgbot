#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <curl/curl.h>
#include <cjson/cJSON.h>

#include "log.h"
#include "requests.h"

typedef struct
{
    char *data;
    size_t size;
}
ServerResponse;

static size_t write_callback(void *data,
                             const size_t data_size,
                             const size_t data_count,
                             void *server_response);

void init_requests_module(void)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

cJSON *get_updates(const int_fast32_t update_id)
{
    CURL *curl = curl_easy_init();

    if (!curl)
        die("%s: %s: failed to initialize curl",
            __BASE_FILE__,
            __func__);

    ServerResponse response;
    response.data = malloc(1);

    if (!response.data)
        die("%s: %s: failed to allocate memory for response.data",
            __BASE_FILE__,
            __func__);

    response.size = 0;

    char url[MAX_URL_SIZE];
    snprintf(url,
             sizeof url,
             "%s/getUpdates?offset=%" PRIdFAST32
             "&timeout=%d",
             BOT_API_URL,
             update_id,
             MAX_RESPONSE_TIMEOUT);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, MAX_CONNECT_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, MAX_RESPONSE_TIMEOUT);

    CURLcode code;
    int retries = 0;

    do
    {
        code = curl_easy_perform(curl);

        if (code == CURLE_OK)
            break;
    }
    while (++retries < MAX_REQUEST_RETRIES);

    curl_easy_cleanup(curl);

    if (code != CURLE_OK)
    {
        free(response.data);
        return NULL;
    }

    cJSON *updates = cJSON_Parse(response.data);

    if (!updates)
        die("%s: %s: failed to parse response.data",
            __BASE_FILE__,
            __func__);

    free(response.data);
    return updates;
}

void leave_chat(const int_fast64_t chat_id)
{
    CURL *curl = curl_easy_init();

    if (!curl)
        die("%s: %s: failed to initialize curl",
            __BASE_FILE__,
            __func__);

    char post_fields[MAX_POSTFIELDS_SIZE];
    snprintf(post_fields,
             sizeof post_fields,
             "chat_id=%" PRIdFAST64,
             chat_id);

    curl_easy_setopt(curl, CURLOPT_URL, BOT_API_URL "/leaveChat");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, MAX_CONNECT_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, MAX_RESPONSE_TIMEOUT);

    int retries = 0;

    do
        if (curl_easy_perform(curl) == CURLE_OK)
            break;
    while (++retries < MAX_REQUEST_RETRIES);

    curl_easy_cleanup(curl);
}

void send_message_with_keyboard(const int_fast64_t chat_id, const char *message, const char *keyboard)
{
    CURL *curl = curl_easy_init();

    if (!curl)
        die("%s: %s: failed to initialize curl",
            __BASE_FILE__,
            __func__);

    char *escaped_message = curl_easy_escape(curl, message, 0);

    if (!escaped_message)
        die("%s: %s: failed to escape message",
            __BASE_FILE__,
            __func__);

    char post_fields[MAX_POSTFIELDS_SIZE];
    snprintf(post_fields,
             sizeof post_fields,
             "chat_id=%" PRIdFAST64
             "&text=%s"
             "&reply_markup=%s",
             chat_id,
             escaped_message,
             keyboard);

    curl_easy_setopt(curl, CURLOPT_URL, BOT_API_URL "/sendMessage");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, MAX_CONNECT_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, MAX_RESPONSE_TIMEOUT);

    int retries = 0;

    do
        if (curl_easy_perform(curl) == CURLE_OK)
            break;
    while (++retries < MAX_REQUEST_RETRIES);

    curl_free(escaped_message);
    curl_easy_cleanup(curl);
}

void answer_callback_query(const char *callback_query_id)
{
    CURL *curl = curl_easy_init();

    if (!curl)
        die("%s: %s: failed to initialize curl",
            __BASE_FILE__,
            __func__);

    char post_fields[MAX_POSTFIELDS_SIZE];
    snprintf(post_fields,
             sizeof post_fields,
             "callback_query_id=%s",
             callback_query_id);

    curl_easy_setopt(curl, CURLOPT_URL, BOT_API_URL "/answerCallbackQuery");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, MAX_CONNECT_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, MAX_RESPONSE_TIMEOUT);

    int retries = 0;

    do
        if (curl_easy_perform(curl) == CURLE_OK)
            break;
    while (++retries < MAX_REQUEST_RETRIES);

    curl_easy_cleanup(curl);
}

static size_t write_callback(void *data,
                             const size_t data_size,
                             const size_t data_count,
                             void *server_response)
{
    const size_t data_real_size = data_size * data_count;

    ServerResponse *response = server_response;
    response->data = realloc(response->data, response->size + data_real_size + 1);

    if (!response->data)
        die("%s: %s: failed to reallocate memory for response->data",
            __BASE_FILE__,
            __func__);

    memcpy(&(response->data[response->size]), data, data_real_size);

    response->size += data_real_size;
    response->data[response->size] = 0;

    return data_real_size;
}
