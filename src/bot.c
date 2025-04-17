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
                                   EMOJI_OK " Действие отменено",
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
                               EMOJI_INFO " ФУРНИТУРА\n"
                               "Фурнитура — это детали и механизмы для сборки и крепления конструкций.\n\n"
                               EMOJI_INFO " ВИДЫ ФУРНИТУРЫ\n"
                               "- Петли: соединяют подвижные элементы (двери, окна)\n"
                               "- Замки: обеспечивают безопасность\n"
                               "- Ручки: для управления подвижными элементами\n"
                               "- Направляющие и ролики: для плавного движения\n"
                               "- Газлифт: плавное закрытие дверей\n"
                               "- Автоматика: дистанционное управление\n\n"
                               EMOJI_INFO " МАТЕРИАЛЫ\n"
                               "- Древесина: каркасы, фасады\n"
                               "- Фанера: фасады, стенки\n"
                               "- ДСП, МДФ: стенки, днища, фасады\n"
                               "- Стекло, металл, пластик: декоративные и опорные элементы\n"
                               "- Ткань: обивка и чехлы\n\n"
                               EMOJI_INFO " КРЕПЕЖ\n"
                               "- Саморезы: для дерева\n"
                               "- Евровинт: с шестигранной головкой\n"
                               "- Шканты: деревянные стержни\n"
                               "- Стяжка: фиксация и выравнивание\n"
                               "- Эксцентрик: регулировка положения\n\n"
                               EMOJI_INFO " КРОМКА\n"
                               "- ПВХ, меламиновая: защита и декор\n"
                               "- Алюминиевая, акриловая: стойкость к воздействиям\n"
                               "- Деревянная, с пленкой или фрезеровкой: дизайн\n\n"
                               EMOJI_INFO " ФУНКЦИИ ПРОЕКТИРОВАНИЯ\n"
                               "1. Растянуть и сдвинуть элементы\n"
                               "2. Растянуть выделенные элементы\n"
                               "3. Выделение окном (в зависимости от направления)\n\n"
                               EMOJI_INFO " ФУНКЦИИ КОПИРОВАНИЯ\n"
                               "- Копировать: вставка в другой файл\n"
                               "- По точкам: внутри одного файла, возможен поворот\n\n"
                               EMOJI_INFO " РЕКОМЕНДАЦИИ ПО КРЕПЛЕНИЯМ\n"
                               "- До 200 мм: 1 крепление\n"
                               "- 200–700 мм: 2 крепления\n"
                               "- 700–1200 мм: 3 крепления\n"
                               "- 1200–2000 мм: 4 крепления\n"
                               "- Более 2000 мм: 5 креплений\n\n"
                               EMOJI_INFO " ПЕТЛИ\n"
                               "- До 950 мм: 2 петли\n"
                               "- 950–1500 мм: 3 петли\n"
                               "- 1500–2000 мм: 4 петли\n"
                               "- Более 2000 мм: 5 петель\n\n"
                               EMOJI_INFO " ПРИМЕЧАНИЕ\n"
                               "Расстановку крепежа можно редактировать в модуле Базис-Мебельщик.",
                               "");
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
