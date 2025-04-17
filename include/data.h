#ifndef DATA_H
    #define DATA_H

    #include <stdint.h>

    #include <cjson/cJSON.h>

    #define FILE_USERS "/var/lib/bolochagina-tgbot/users.json"

    #define MAX_USERNAME_SIZE 32
    #define MAX_CHAT_ID_SIZE  20
    #define MAX_QUESTION_SIZE 1024

    void init_data_module(void);
    int has_user(const int_fast64_t chat_id);
    void create_user(const int_fast64_t chat_id);
    int get_state(const int_fast64_t chat_id, const char *state_name);
    void set_state(const int_fast64_t chat_id, const char *state_name, const int state_value);
    int has_question(const int_fast64_t chat_id);
    void create_question(const int_fast64_t chat_id, const char *question_text);
    void delete_question(const int_fast64_t chat_id);
    cJSON *get_questions(void);

#endif
