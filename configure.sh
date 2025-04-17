#!/usr/bin/env bash

readonly CONFIG_FILE='include/config.h'
readonly ERRORSTAMP='\e[0;31;1mError:\e[0m'

if [[ -f $CONFIG_FILE ]]; then
    bot_token=$(sed -nE 's/^\s*#define BOT_TOKEN\s+"(.*)"/\1/p' $CONFIG_FILE)
    root_chat_id=$(sed -nE 's/^\s*#define ROOT_CHAT_ID\s+(.+)/\1/p' $CONFIG_FILE)
fi

echo -e '\e[0;33;1mConfiguring bolochagina-tgbot...\e[0m'

read -p 'Telegram bot token: ' new_bot_token

if [[ -z $new_bot_token && -n $bot_token ]]; then
    echo -e '\e[0;33;1mUsing existing telegram bot token...\e[0m'
else
    bot_token=${new_bot_token:-$bot_token}

    if [[ -z $bot_token ]]; then
        echo -e "$ERRORSTAMP telegram bot token is empty"
        exit 1
    fi
fi

read -p 'Administrator user id: ' new_root_chat_id

if [[ -z $new_root_chat_id && -n $root_chat_id ]]; then
    echo -e '\e[0;33;1mUsing existing administrator user id...\e[0m'
else
    root_chat_id=${new_root_chat_id:-$root_chat_id}

    if [[ -z $root_chat_id ]]; then
        echo -e "$ERRORSTAMP administrator user id is empty"
        exit 1
    fi
fi

echo "// This file is autogenerated by configure.sh.

#ifndef CONFIG_H
    #define CONFIG_H

    #define BOT_TOKEN    \"$bot_token\"
    #define ROOT_CHAT_ID $root_chat_id

#endif" > $CONFIG_FILE

echo -e '\e[0;32;1mConfiguration done!\e[0m'
