#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>

#include <cjson/cJSON.h>

#include "log.h"
#include "data.h"

static void load_users(void);
static void save_users(void);

static cJSON *users_cache;
static pthread_rwlock_t users_cache_rwlock = PTHREAD_RWLOCK_INITIALIZER;

void init_data_module(void)
{
    load_users();
}

int has_user(const int_fast64_t chat_id)
{
    char chat_id_string[MAX_CHAT_ID_SIZE + 1];
    snprintf(chat_id_string,
             sizeof chat_id_string,
             "%" PRIdFAST64,
             chat_id);

    pthread_rwlock_rdlock(&users_cache_rwlock);
    const int state = cJSON_GetObjectItem(users_cache, chat_id_string) ? 1 : 0;
    pthread_rwlock_unlock(&users_cache_rwlock);

    return state;
}

void create_user(const int_fast64_t chat_id)
{
    char chat_id_string[MAX_CHAT_ID_SIZE + 1];
    snprintf(chat_id_string,
             sizeof chat_id_string,
             "%" PRIdFAST64,
             chat_id);

    cJSON *user = cJSON_CreateObject();
    cJSON_AddNumberToObject(user, "question_description_state", 0);

    pthread_rwlock_wrlock(&users_cache_rwlock);

    cJSON_AddItemToObject(users_cache, chat_id_string, user);
    save_users();

    pthread_rwlock_unlock(&users_cache_rwlock);
}

int get_state(const int_fast64_t chat_id, const char *state_name)
{
    char chat_id_string[MAX_CHAT_ID_SIZE + 1];
    snprintf(chat_id_string,
             sizeof chat_id_string,
             "%" PRIdFAST64,
             chat_id);

    pthread_rwlock_rdlock(&users_cache_rwlock);
    const cJSON *state = cJSON_GetObjectItem(cJSON_GetObjectItem(users_cache, chat_id_string), state_name);
    pthread_rwlock_unlock(&users_cache_rwlock);

    return state->valueint;
}

void set_state(const int_fast64_t chat_id, const char *state_name, const int state_value)
{
    char chat_id_string[MAX_CHAT_ID_SIZE + 1];
    snprintf(chat_id_string,
             sizeof chat_id_string,
             "%" PRIdFAST64,
             chat_id);

    pthread_rwlock_wrlock(&users_cache_rwlock);

    cJSON_SetIntValue(cJSON_GetObjectItem(cJSON_GetObjectItem(users_cache, chat_id_string), state_name), state_value);
    save_users();

    pthread_rwlock_unlock(&users_cache_rwlock);
}

int has_question(const int_fast64_t chat_id)
{
    char chat_id_string[MAX_CHAT_ID_SIZE + 1];
    snprintf(chat_id_string,
             sizeof chat_id_string,
             "%" PRIdFAST64,
             chat_id);

    pthread_rwlock_rdlock(&users_cache_rwlock);
    const int state = cJSON_GetObjectItem(cJSON_GetObjectItem(users_cache, chat_id_string), "question") ? 1 : 0;
    pthread_rwlock_unlock(&users_cache_rwlock);

    return state;
}

void create_question(const int_fast64_t chat_id, const char *question_text)
{
    char chat_id_string[MAX_CHAT_ID_SIZE + 1];
    snprintf(chat_id_string,
             sizeof chat_id_string,
             "%" PRIdFAST64,
             chat_id);

    cJSON *question = cJSON_CreateObject();

    cJSON_AddStringToObject(question, "text", question_text);

    pthread_rwlock_wrlock(&users_cache_rwlock);

    cJSON_AddItemToObject(cJSON_GetObjectItem(users_cache, chat_id_string), "question", question);
    save_users();

    pthread_rwlock_unlock(&users_cache_rwlock);
}

void delete_question(const int_fast64_t chat_id)
{
    char chat_id_string[MAX_CHAT_ID_SIZE + 1];
    snprintf(chat_id_string,
             sizeof chat_id_string,
             "%" PRIdFAST64,
             chat_id);

    pthread_rwlock_wrlock(&users_cache_rwlock);

    cJSON_DeleteItemFromObject(cJSON_GetObjectItem(users_cache, chat_id_string), "question");
    save_users();

    pthread_rwlock_unlock(&users_cache_rwlock);
}

cJSON *get_questions(void)
{
    cJSON *questions = cJSON_CreateArray();

    pthread_rwlock_rdlock(&users_cache_rwlock);
    cJSON *user = users_cache->child;

    while (user)
    {
        const cJSON *question = cJSON_GetObjectItem(user, "question");

        if (question)
        {
            cJSON *text = cJSON_GetObjectItem(question, "text");

            char chat_id_with_question[MAX_CHAT_ID_SIZE + MAX_USERNAME_SIZE + MAX_QUESTION_SIZE + 7];
            snprintf(chat_id_with_question,
                     sizeof chat_id_with_question,
                     "(%s) %s",
                     user->string,
                     text->valuestring);

            cJSON_AddItemToArray(questions, cJSON_CreateString(chat_id_with_question));
        }

        user = user->next;
    }

    pthread_rwlock_unlock(&users_cache_rwlock);
    return questions;
}

static void load_users(void)
{
    FILE *users_file = fopen(FILE_USERS, "r");

    if (!users_file)
        die("%s: %s: failed to open %s",
            __BASE_FILE__,
            __func__,
            FILE_USERS);

    fseek(users_file, 0, SEEK_END);
    const size_t users_file_size = ftell(users_file);
    rewind(users_file);

    char *users_string = malloc(users_file_size + 1);

    if (!users_string)
        die("%s: %s: failed to allocate memory for users_string",
            __BASE_FILE__,
            __func__);

    if (fread(users_string,
              1,
              users_file_size,
              users_file) != users_file_size)
        die("%s: %s: failed to read data from %s",
            __BASE_FILE__,
            __func__,
            FILE_USERS);

    fclose(users_file);

    users_string[users_file_size] = 0;
    cJSON *users = cJSON_Parse(users_string);

    if (!users)
        die("%s: %s: failed to parse users_string",
            __BASE_FILE__,
            __func__);

    free(users_string);
    users_cache = users;
}

static void save_users(void)
{
    char *users_string = cJSON_PrintUnformatted(users_cache);

    if (!users_string)
        die("%s: %s: failed to print users_cache",
            __BASE_FILE__,
            __func__);

    FILE *users_file = fopen(FILE_USERS, "w");

    if (!users_file)
        die("%s: %s: failed to open %s",
            __BASE_FILE__,
            __func__,
            FILE_USERS);

    fprintf(users_file, "%s", users_string);

    fclose(users_file);
    free(users_string);
}
