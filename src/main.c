#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "version.h"
#include "log.h"
#include "requests.h"
#include "data.h"
#include "bot.h"

#define ERRORSTAMP "\e[0;31;1mError:\e[0m"

#define DIR_LOCK  "/var/run/bolochagina-tgbot/"
#define FILE_LOCK "bolochagina-tgbot.lock"

static void handle_args(int argc, char **argv);
static void init_pw(void);
static void check_instance(void);
static void drop_privileges(void);
static void daemonize(void);
static void init_signals(void);
static void init_modules(void);
static void init_info(void);
static void handle_signal(const int signal);

static int maintenance_mode = 0;

static struct passwd *pw;

static pid_t pid;
static char *mode;

int main(int argc, char **argv)
{
    handle_args(argc, argv);

    init_pw();
    check_instance();
    drop_privileges();

    daemonize();

    init_signals();
    init_modules();
    init_info();

    report("bolochagina-tgbot %d.%d.%d started (PID: %d; Mode: %s)",
           MAJOR_VERSION,
           MINOR_VERSION,
           PATCH_VERSION,
           pid,
           mode);

    start_bot(maintenance_mode);
}

static void handle_args(int argc, char **argv)
{
    if (argc == 1)
        return;

    opterr = 0;

    static const struct option long_options[] =
    {
        {"help",        no_argument, 0, 'h'},
        {"version",     no_argument, 0, 'v'},
        {"maintenance", no_argument, 0, 'm'},
        {0, 0, 0, 0}
    };

    int opt;

    while ((opt = getopt_long(argc,
                              argv,
                              "+hvm",
                              long_options,
                              NULL)) != -1)
    {
        switch (opt)
        {
            case 'h':
                printf("Usage: bolochagina-tgbot [option]\n"
                       "Daemon for controlling the 'Help for students' Telegram bot.\n\n"
                       "Options:\n"
                       "  -h, --help           print this help and exit\n"
                       "  -v, --version        print the bolochagina-tgbot version and exit\n"
                       "  -m, --maintenance    run the bolochagina-tgbot in maintenance mode\n"
                       "\nTo run the bolochagina-tgbot, run it with the superuser privileges."
                       "\nbolochagina-tgbot will automatically drop privileges to the bolochagina-tgbot user.\n");
                exit(EXIT_SUCCESS);

            case 'v':
                printf("bolochagina-tgbot %d.%d.%d\n",
                       MAJOR_VERSION,
                       MINOR_VERSION,
                       PATCH_VERSION);
                exit(EXIT_SUCCESS);

            case 'm':
                maintenance_mode = 1;
                break;

            case '?':
                if (optopt)
                    fprintf(stderr,
                            ERRORSTAMP " unknown option '-%c'\n"
                            "Try 'bolochagina-tgbot -h' for more information.\n",
                            optopt);
                else
                    fprintf(stderr,
                            ERRORSTAMP " unknown option '%s'\n"
                            "Try 'bolochagina-tgbot -h' for more information.\n",
                            argv[optind - 1]);

                // Fall through.

            default:
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
    {
        fprintf(stderr,
                ERRORSTAMP " expecting options only\n"
                "Try 'bolochagina-tgbot -h' for more information.\n");
        exit(EXIT_FAILURE);
    }
}

static void init_pw(void)
{
    if (!(pw = getpwnam("bolochagina-tgbot")))
    {
        fprintf(stderr,
                ERRORSTAMP " failed to get bolochagina-tgbot user data\n");
        exit(EXIT_FAILURE);
    }
}

static void check_instance(void)
{
    if (access(DIR_LOCK, F_OK))
    {
        if (mkdir(DIR_LOCK, 0700))
        {
            fprintf(stderr,
                    ERRORSTAMP " failed to create %s\n",
                    DIR_LOCK);
            exit(EXIT_FAILURE);
        }
        else if (chown(DIR_LOCK, pw->pw_uid, pw->pw_gid))
        {
            fprintf(stderr,
                    ERRORSTAMP " failed to change %s ownership\n",
                    DIR_LOCK);
            exit(EXIT_FAILURE);
        }
    }

    int fd = open(DIR_LOCK FILE_LOCK, O_CREAT | O_RDWR | O_EXCL, 0644);

    if (fd >= 0)
    {
        if (fchown(fd, pw->pw_uid, pw->pw_gid))
        {
            fprintf(stderr,
                    ERRORSTAMP " failed to change %s ownership\n",
                    DIR_LOCK FILE_LOCK);
            exit(EXIT_FAILURE);
        }
    }
    else if (errno == EEXIST)
    {
        if ((fd = open(DIR_LOCK FILE_LOCK, O_RDWR)) < 0)
        {
            fprintf(stderr,
                    ERRORSTAMP " failed to open %s\n",
                    DIR_LOCK FILE_LOCK);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        fprintf(stderr,
                ERRORSTAMP " failed to open or create %s\n",
                DIR_LOCK FILE_LOCK);
        exit(EXIT_FAILURE);
    }

    if (flock(fd, LOCK_EX | LOCK_NB))
    {
        if (errno == EWOULDBLOCK)
            fprintf(stderr,
                    ERRORSTAMP " bolochagina-tgbot is already running\n");
        else
            fprintf(stderr,
                    ERRORSTAMP " failed to lock %s\n",
                    DIR_LOCK FILE_LOCK);

        exit(EXIT_FAILURE);
    }
}

static void drop_privileges(void)
{
    if (setgid(pw->pw_gid) < 0)
    {
        fprintf(stderr,
                ERRORSTAMP " failed to set bolochagina-tgbot user GID\n");
        exit(EXIT_FAILURE);
    }

    if (setuid(pw->pw_uid) < 0)
    {
        fprintf(stderr,
                ERRORSTAMP " failed to set bolochagina-tgbot user UID\n");
        exit(EXIT_FAILURE);
    }
}

static void daemonize(void)
{
    if (daemon(0, 0) < 0)
    {
        fprintf(stderr,
                ERRORSTAMP " failed to daemonize\n");
        exit(EXIT_FAILURE);
    }
}

static void init_signals(void)
{
    signal(SIGTERM, handle_signal);
    signal(SIGSEGV, handle_signal);
}

static void init_modules(void)
{
    init_requests_module();

    if (!maintenance_mode)
        init_data_module();
}

static void init_info(void)
{
    pid = getpid();
    mode = maintenance_mode ? "Maintenance" : "Default";
}

static void handle_signal(const int signal)
{
    switch (signal)
    {
        case SIGTERM:
            report("bolochagina-tgbot %d.%d.%d terminated (PID: %d; Mode: %s)",
                   MAJOR_VERSION,
                   MINOR_VERSION,
                   PATCH_VERSION,
                   pid,
                   mode);
            exit(EXIT_SUCCESS);

        case SIGSEGV:
            die("Segmentation fault");
    }
}
