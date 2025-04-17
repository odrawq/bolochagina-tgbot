#ifndef LOG_H
    #define LOG_H

    #define FILE_INFOLOG  "/var/log/bolochagina-tgbot/info_log"
    #define FILE_ERRORLOG "/var/log/bolochagina-tgbot/error_log"

    #define MAX_TIMESTAMP_SIZE 21

    void report(const char *fmt, ...);
    void die(const char *fmt, ...);

#endif
