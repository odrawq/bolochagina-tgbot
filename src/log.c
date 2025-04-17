#include <pthread.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "log.h"

static pthread_mutex_t info_log_mutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t error_log_mutex = PTHREAD_MUTEX_INITIALIZER;

void report(const char *fmt, ...)
{
    pthread_mutex_lock(&info_log_mutex);

    FILE *info_log = fopen(FILE_INFOLOG, "a");

    if (!info_log)
        die("%s: %s: failed to open %s",
            __BASE_FILE__,
            __func__,
            FILE_INFOLOG);

    const time_t current_time = time(NULL);

    char timestamp[MAX_TIMESTAMP_SIZE + 1];
    strftime(timestamp,
             sizeof timestamp,
             "[%Y-%m-%d %H:%M:%S]",
             localtime(&current_time));

    fprintf(info_log, "%s ", timestamp);
    va_list argp;
    va_start(argp, fmt);
    vfprintf(info_log, fmt, argp);
    va_end(argp);
    fputc('\n', info_log);

    fclose(info_log);

    pthread_mutex_unlock(&info_log_mutex);
}

void die(const char *fmt, ...)
{
    pthread_mutex_lock(&error_log_mutex);

    FILE *error_log = fopen(FILE_ERRORLOG, "a");

    if (error_log)
    {
        const time_t current_time = time(NULL);

        char timestamp[MAX_TIMESTAMP_SIZE + 1];
        strftime(timestamp,
                 sizeof timestamp,
                 "[%Y-%m-%d %H:%M:%S]",
                 localtime(&current_time));

        fprintf(error_log, "%s ", timestamp);
        va_list argp;
        va_start(argp, fmt);
        vfprintf(error_log, fmt, argp);
        va_end(argp);
        fputc('\n', error_log);

        fclose(error_log);
    }

    exit(EXIT_FAILURE);
}
