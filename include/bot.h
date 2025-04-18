#ifndef BOT_H
    #define BOT_H

    #define EMOJI_OK        "\U00002705"
    #define EMOJI_FAILED    "\U0000274C"
    #define EMOJI_SEARCH    "\U0001F50E"
    #define EMOJI_ATTENTION "\U00002757"
    #define EMOJI_QUESTION  "\U00002753"
    #define EMOJI_WRITE     "\U0001F58A"
    #define EMOJI_GREETING  "\U0001F44B"
    #define EMOJI_INFO      "\U00002139"

    #define COMMAND_CANCEL EMOJI_FAILED    " Отменить"
    #define COMMAND_FAQ    EMOJI_SEARCH    " Часто задаваемые вопросы"
    #define COMMAND_ASK    EMOJI_ATTENTION " Задать вопрос"
    #define COMMAND_START  "/start"
    #define COMMAND_LIST   "/ls"
    #define COMMAND_REMOVE "/rm"

    #define MAX_COMMAND_REMOVE_SIZE 3

    #define FAQ_INLINEKEYBOARD "{\"inline_keyboard\":[" \
                               "[{\"text\":\"Фурнитура\",\"callback_data\":\"fittings\"}]," \
                               "[{\"text\":\"Материалы\",\"callback_data\":\"materials\"}]," \
                               "[{\"text\":\"Крепёж\",\"callback_data\":\"fasteners\"}]," \
                               "[{\"text\":\"Кромка\",\"callback_data\":\"edge_band\"}]," \
                               "[{\"text\":\"Функции проектирования\",\"callback_data\":\"design_functions\"}]," \
                               "[{\"text\":\"Копирование\",\"callback_data\":\"copying\"}]," \
                               "[{\"text\":\"Количество креплений\",\"callback_data\":\"fastener_count\"}]" \
                               "]}"

    #define NOKEYBOARD "{\"remove_keyboard\":true}"

    #define get_current_keyboard(chat_id) (has_question(chat_id) ? \
                                           "{\"keyboard\":[[{\"text\":\"" COMMAND_FAQ "\"}]],\"resize_keyboard\":true}" : \
                                           (get_state(chat_id, "question_description_state") ? \
                                            "{\"keyboard\":[[{\"text\":\"" COMMAND_CANCEL "\"}]],\"resize_keyboard\":true}" : \
                                            "{\"keyboard\":[[{\"text\":\"" COMMAND_FAQ "\"},{\"text\":\"" COMMAND_ASK "\"}]],\"resize_keyboard\":true}"))

    void start_bot(const int maintenance_mode);

#endif
