#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include <cjson/cJSON.h>

#include "config.h"
#include "log.h"
#include "requests.h"
#include "data.h"
#include "bot.h"

static void handle_updates(cJSON *updates, const int maintenance_mode);
static void *handle_message_in_maintenance_mode(void *cjson_message);
static void *handle_message_in_default_mode(void *cjson_message);
static void *handle_callback_query_in_maintenance_mode(void *cjson_callback_query);
static void *handle_callback_query_in_default_mode(void *cjson_callback_query);
static void handle_question(const int_fast64_t chat_id,
                            const int root_access,
                            const char *username,
                            const char *question);
static void handle_command(const int_fast64_t chat_id,
                           const int root_access,
                           const char *username,
                           const char *command);
static void handle_faq_command(const int_fast64_t chat_id);
static void handle_ask_command(const int_fast64_t chat_id, const char *username);
static void handle_start_command(const int_fast64_t chat_id, const int root_access, const char *username);
static void handle_list_command(const int_fast64_t chat_id, const int root_access);
static void handle_remove_command(const int_fast64_t chat_id, const int root_access, const char *arg);

static int_fast32_t last_update_id = 0;

void start_bot(const int maintenance_mode)
{
    for (;;)
    {
        cJSON *updates = get_updates(last_update_id);

        if (updates)
        {
            handle_updates(updates, maintenance_mode);
            cJSON_Delete(updates);
        }
    }
}

static void handle_updates(cJSON *updates, const int maintenance_mode)
{
    const cJSON *result = cJSON_GetObjectItem(updates, "result");
    const int result_size = cJSON_GetArraySize(result);

    for (int i = 0; i < result_size; ++i)
    {
        const cJSON *update = cJSON_GetArrayItem(result, i);
        last_update_id = cJSON_GetNumberValue(cJSON_GetObjectItem(update, "update_id")) + 1;

        const cJSON *message = cJSON_GetObjectItem(update, "message");

        if (message)
        {
            pthread_t handle_message_thread;

            if (pthread_create(&handle_message_thread,
                               NULL,
                               maintenance_mode ? handle_message_in_maintenance_mode : handle_message_in_default_mode,
                               cJSON_Duplicate(message, 1)))
                die("%s: %s: failed to create handle_message_thread",
                    __BASE_FILE__,
                    __func__);

            pthread_detach(handle_message_thread);
        }

        const cJSON *callback_query = cJSON_GetObjectItem(update, "callback_query");

        if (callback_query)
        {
            pthread_t handle_callback_query_thread;

            if (pthread_create(&handle_callback_query_thread,
                               NULL,
                               maintenance_mode ? handle_callback_query_in_maintenance_mode : handle_callback_query_in_default_mode,
                               cJSON_Duplicate(callback_query, 1)))
                die("%s: %s: failed to create handle_callback_query_thread",
                    __BASE_FILE__,
                    __func__);

            pthread_detach(handle_callback_query_thread);
        }
    }
}

static void *handle_message_in_maintenance_mode(void *cjson_message)
{
    cJSON *message = cjson_message;

    send_message_with_keyboard(cJSON_GetNumberValue(cJSON_GetObjectItem(cJSON_GetObjectItem(message, "chat"), "id")),
                               EMOJI_FAILED " Извините, бот временно недоступен\n\n"
                               "Проводятся технические работы. Пожалуйста, ожидайте!",
                               "");

    cJSON_Delete(message);
    return NULL;
}

static void *handle_message_in_default_mode(void *cjson_message)
{
    cJSON *message = cjson_message;

    const cJSON *chat = cJSON_GetObjectItem(message, "chat");
    const int_fast64_t chat_id = cJSON_GetNumberValue(cJSON_GetObjectItem(chat, "id"));

    if (strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(chat, "type")), "private"))
    {
        send_message_with_keyboard(chat_id,
                                   EMOJI_FAILED " Извините, я могу работать только в личных сообщениях",
                                   "");
        leave_chat(chat_id);
        goto exit;
    }

    const int root_access = (chat_id == ROOT_CHAT_ID);

    if (!has_user(chat_id))
    {
        create_user(chat_id);
        report("New user %" PRIdFAST64
               " appeared",
               chat_id);
    }

    const cJSON *username = cJSON_GetObjectItem(chat, "username");
    const cJSON *text = cJSON_GetObjectItem(message, "text");

    if (get_state(chat_id, "question_description_state"))
        handle_question(chat_id,
                        root_access,
                        username ? username->valuestring : NULL,
                        text ? text->valuestring : NULL);
    else
        handle_command(chat_id,
                       root_access,
                       username ? username->valuestring : NULL,
                       text ? text->valuestring : NULL);

exit:
    cJSON_Delete(message);
    return NULL;
}

static void *handle_callback_query_in_maintenance_mode(void *cjson_callback_query)
{
    cJSON *callback_query = cjson_callback_query;

    answer_callback_query(cJSON_GetStringValue(cJSON_GetObjectItem(callback_query, "id")));

    send_message_with_keyboard(cJSON_GetNumberValue(cJSON_GetObjectItem(cJSON_GetObjectItem(callback_query, "from"), "id")),
                               EMOJI_FAILED " Извините, бот временно недоступен\n\n"
                               "Проводятся технические работы. Пожалуйста, ожидайте!",
                               "");

    cJSON_Delete(callback_query);
    return NULL;
}

static void *handle_callback_query_in_default_mode(void *cjson_callback_query)
{
    cJSON *callback_query = cjson_callback_query;

    const char *callback_query_id = cJSON_GetStringValue(cJSON_GetObjectItem(callback_query, "id"));
    const char *callback_query_data = cJSON_GetStringValue(cJSON_GetObjectItem(callback_query, "data"));

    answer_callback_query(callback_query_id);

    const int_fast64_t chat_id = cJSON_GetNumberValue(cJSON_GetObjectItem(cJSON_GetObjectItem(callback_query, "from"), "id"));

    if (!strcmp(callback_query_data, "fittings"))
        send_message_with_keyboard(chat_id,
                                   EMOJI_INFO " Фурнитура - это различные детали и механизмы для сборки и крепления конструкций\n\n"
                                   "- Петли: для соединения подвижных элементов (двери, окна, крышки);\n"
                                   "- Замки: для безопасности и блокировки;\n"
                                   "- Ручки: для управления подвижными частями;\n"
                                   "- Направляющие и ролики: для плавного движения;\n"
                                   "- Газлифт: для мягкого закрытия дверей;\n"
                                   "- Автоматические системы: дистанционное управление дверьми.",
                                   "");
    else if (!strcmp(callback_query_data, "materials"))
        send_message_with_keyboard(chat_id,
                                   EMOJI_INFO " Материалы в мебельном производстве\n\n"
                                   "- Древесина: каркасы, фасады, столешницы;\n"
                                   "- Фанера: фасады, задние стенки;\n"
                                   "- ДСП: задние стенки, днища ящиков;\n"
                                   "- МДФ: фасады, задние стенки;\n"
                                   "- Стекло: фасады, столешницы;\n"
                                   "- Металл: каркасы, опоры, ножки;\n"
                                   "- Пластик: задние стенки, днища ящиков;\n"
                                   "- Ткань: обивка мебели, чехлы.",
                                   "");
    else if (!strcmp(callback_query_data, "fasteners"))
        send_message_with_keyboard(chat_id,
                                   EMOJI_INFO " Крепёж - это элементы для соединения частей конструкций\n\n"
                                   "- Саморезы: для деревянных деталей;\n"
                                   "- Евровинт: с шестигранной головкой;\n"
                                   "- Шканты: цилиндры из дерева;\n"
                                   "- Стяжка: фиксация и выравнивание элементов;\n"
                                   "- Эксцентрик: регулировка положения элементов.\n\n"
                                   "Редактирование крепежа осуществляется в модуле Базис-Мебельщик.",
                                   "");
    else if (!strcmp(callback_query_data, "edge_band"))
        send_message_with_keyboard(chat_id,
                                   EMOJI_INFO " Кромка - материал для закрытия торцов панелей\n\n"
                                   "- ПВХ-кромка: для ЛДСП, МДФ;\n"
                                   "- Меламиновая: устойчива к влаге;\n"
                                   "- Алюминиевая: защита от коррозии;\n"
                                   "- Акриловая: устойчива к химии;\n"
                                   "- Кромка из дерева: элегантный внешний вид;\n"
                                   "- Кромка с плёнкой: декоративные варианты;\n"
                                   "- Кромка с фрезеровкой: оригинальный дизайн.",
                                   "");
    else if (!strcmp(callback_query_data, "design_functions"))
        send_message_with_keyboard(chat_id,
                                   EMOJI_INFO " Функции проектирования\n\n"
                                   "- 'Растянуть и сдвинуть элементы': выделите область, укажите точку и переместите;\n"
                                   "- 'Растянуть и сдвинуть выделенные элементы': работает только с выделенными объектами;\n"
                                   "- 'Выделить окном': выделение элементов в зависимости от направления движения мыши.",
                                   "");
    else if (!strcmp(callback_query_data, "copying"))
        send_message_with_keyboard(chat_id,
                                   EMOJI_INFO " Функции копирования\n\n"
                                   "- 'Копировать': вставка в другой файл;\n"
                                   "- 'Копировать по точкам': внутри одного файла, возможен поворот и отражение.",
                                   "");
    else if (!strcmp(callback_query_data, "fastener_count"))
        send_message_with_keyboard(chat_id,
                                   EMOJI_INFO " Количество креплений\n\n"
                                   "- До 200 мм: 1 крепление;\n"
                                   "- 200–700 мм: 2 крепления;\n"
                                   "- 700–1200 мм: 3 крепления;\n"
                                   "- 1200–2000 мм: 4 крепления;\n"
                                   "- Более 2000 мм: 5 креплений.\n\n"
                                   EMOJI_INFO " Количество петель\n\n"
                                   "- До 950 мм: 2 петли;\n"
                                   "- 950–1500 мм: 3 петли;\n"
                                   "- 1500–2000 мм: 4 петли;\n"
                                   "- Более 2000 мм: 5 петель.",
                                   "");

    cJSON_Delete(callback_query);
    return NULL;
}

static void handle_question(const int_fast64_t chat_id,
                            const int root_access,
                            const char *username,
                            const char *question)
{
    if (!question)
    {
        send_message_with_keyboard(chat_id,
                                   EMOJI_FAILED " Извините, я понимаю только текст",
                                   "");
        return;
    }

    if (!strcmp(question, COMMAND_CANCEL))
    {
        set_state(chat_id, "question_description_state", 0);
        send_message_with_keyboard(chat_id,
                                   EMOJI_OK " Создание вопроса отменено",
                                   get_current_keyboard(chat_id));
        return;
    }

    if (!username)
    {
        set_state(chat_id, "question_description_state", 0);
        send_message_with_keyboard(chat_id,
                                   EMOJI_FAILED " Извините, для этой функции вам нужно "
                                   "создать имя пользователя в настройках Telegram",
                                   get_current_keyboard(chat_id));
        return;
    }

    if (strlen(question) > MAX_QUESTION_SIZE)
    {
        send_message_with_keyboard(chat_id,
                                   EMOJI_FAILED " Извините, ваш вопрос слишком большой",
                                   "");
        return;
    }

    char username_with_question[MAX_USERNAME_SIZE + MAX_QUESTION_SIZE + 4];
    snprintf(username_with_question,
             sizeof username_with_question,
             "@%s: %s",
             username,
             question);

    create_question(chat_id, username_with_question);
    set_state(chat_id, "question_description_state", 0);

    report("User %" PRIdFAST64
           " with username '%s'"
           " created question '%s'",
           chat_id,
           username,
           question);

    send_message_with_keyboard(chat_id,
                               EMOJI_OK " Ваш вопрос сохранён\n\n"
                               "Надеюсь вам ответят как можно быстрее!",
                               get_current_keyboard(chat_id));

    if (!root_access)
        send_message_with_keyboard(ROOT_CHAT_ID,
                                   EMOJI_INFO " Появился новый вопрос",
                                   "");
}

static void handle_command(const int_fast64_t chat_id,
                           const int root_access,
                           const char *username,
                           const char *command)
{
    if (!command)
    {
        send_message_with_keyboard(chat_id,
                                   EMOJI_FAILED " Извините, я понимаю только текст",
                                   "");
        return;
    }

    if (!strcmp(command, COMMAND_FAQ))
        handle_faq_command(chat_id);
    else if (!strcmp(command, COMMAND_ASK))
        handle_ask_command(chat_id, username);
    else if (!strcmp(command, COMMAND_START))
        handle_start_command(chat_id, root_access, username);
    else if (!strcmp(command, COMMAND_LIST))
        handle_list_command(chat_id, root_access);
    else if (!strncmp(command, COMMAND_REMOVE, MAX_COMMAND_REMOVE_SIZE))
        handle_remove_command(chat_id,
                              root_access,
                              command + MAX_COMMAND_REMOVE_SIZE);
    else
        send_message_with_keyboard(chat_id,
                                   EMOJI_FAILED " Извините, я не знаю такого действия",
                                   "");
}

static void handle_faq_command(const int_fast64_t chat_id)
{
    send_message_with_keyboard(chat_id,
                               EMOJI_QUESTION "Что вас интересует",
                               FAQ_INLINEKEYBOARD);
}

static void handle_ask_command(const int_fast64_t chat_id, const char *username)
{
    if (has_question(chat_id))
        send_message_with_keyboard(chat_id,
                                   EMOJI_FAILED " Извините, вы уже задали вопрос",
                                   "");
    else
    {
        if (!username)
            send_message_with_keyboard(chat_id,
                                       EMOJI_FAILED " Извините, для этой функции вам нужно "
                                       "создать имя пользователя в настройках Telegram",
                                       "");
        else
        {
            set_state(chat_id, "question_description_state", 1);
            send_message_with_keyboard(chat_id,
                                       EMOJI_WRITE " Задайте ваш вопрос",
                                       get_current_keyboard(chat_id));
        }
    }
}

static void handle_start_command(const int_fast64_t chat_id, const int root_access, const char *username)
{
    const char *user_greeting = EMOJI_GREETING " Добро пожаловать";

    char start_message[strlen(user_greeting) + MAX_USERNAME_SIZE + 3];

    if (username)
        snprintf(start_message,
                 sizeof start_message,
                 "%s, @%s",
                 user_greeting,
                 username);
    else
        snprintf(start_message,
                 sizeof start_message,
                 "%s",
                 user_greeting);

    send_message_with_keyboard(chat_id,
                               start_message,
                               get_current_keyboard(chat_id));

    if (root_access)
        send_message_with_keyboard(ROOT_CHAT_ID,
                                   EMOJI_ATTENTION " ВЫ ЯВЛЯЕТЕСЬ АДМИНИСТРАТОРОМ\n\n"
                                   EMOJI_INFO " Вывести список вопросов\n"
                                   "/ls\n\n"
                                   EMOJI_INFO " Удалить вопрос\n"
                                   "/rm <id>\n\n"
                                   "Вместо <id> нужно указать идентификатор чата. "
                                   "Идентификатор находится перед вопросом пользователя в круглых скобках.",
                                   "");
}

static void handle_list_command(const int_fast64_t chat_id, const int root_access)
{
    if (!root_access)
        send_message_with_keyboard(chat_id,
                                   EMOJI_FAILED " Извините, у вас недостаточно прав",
                                   "");
    else
    {
        cJSON *questions = get_questions();
        const int questions_size = cJSON_GetArraySize(questions);

        if (!questions_size)
            send_message_with_keyboard(ROOT_CHAT_ID,
                                       EMOJI_OK " Вопросов не найдено",
                                       "");
        else
            for (int i = 0; i < questions_size; ++i)
                send_message_with_keyboard(ROOT_CHAT_ID,
                                           cJSON_GetStringValue(cJSON_GetArrayItem(questions, i)),
                                           i + 1 != questions_size ? NOKEYBOARD : get_current_keyboard(ROOT_CHAT_ID));

        cJSON_Delete(questions);
    }
}

static void handle_remove_command(const int_fast64_t chat_id, const int root_access, const char *arg)
{
    if (!root_access)
        send_message_with_keyboard(chat_id,
                                   EMOJI_FAILED " Извините, у вас недостаточно прав",
                                   "");
    else
    {
        while (*arg == ' ')
            ++arg;

        if (!*arg)
            send_message_with_keyboard(ROOT_CHAT_ID,
                                       EMOJI_FAILED " Извините, вы не указали идентификатор чата",
                                       "");
        else
        {
            char *end;
            const int_fast64_t target_chat_id = strtoll(arg, &end, 10);

            if (*end || end == arg)
                send_message_with_keyboard(ROOT_CHAT_ID,
                                           EMOJI_FAILED " Извините, вы указали некорректный идентификатор чата",
                                           "");
            else if (!has_user(target_chat_id))
                send_message_with_keyboard(ROOT_CHAT_ID,
                                           EMOJI_FAILED " Извините, такого пользователя не существует",
                                           "");
            else if (!has_question(target_chat_id))
                send_message_with_keyboard(ROOT_CHAT_ID,
                                           EMOJI_FAILED " Извините, у пользователя нет вопроса",
                                           "");
            else
            {
                delete_question(target_chat_id);

                report("User %" PRIdFAST64
                       " deleted user %" PRIdFAST64
                       " question",
                       ROOT_CHAT_ID,
                       target_chat_id);

                if (chat_id != target_chat_id)
                    send_message_with_keyboard(target_chat_id,
                                               EMOJI_OK " Ваш вопрос был решён\n\n"
                                               "Если у вас остались ещё вопросы, не бойтесь задавать их снова!",
                                               get_current_keyboard(target_chat_id));

                send_message_with_keyboard(ROOT_CHAT_ID,
                                           EMOJI_OK " Вопрос удалён",
                                           get_current_keyboard(ROOT_CHAT_ID));
            }
        }
    }
}
