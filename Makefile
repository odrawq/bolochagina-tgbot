SHELL := /usr/bin/env bash

TARGET := bolochagina-tgbot

CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -Iinclude/ -MMD
LDFLAGS := -lpthread -lcurl -lcjson

INIT_DIR    := init/
SRC_DIR     := src/
BUILD_DIR   := build/
SYSTEMD_DIR := /etc/systemd/system/
BIN_DIR     := /usr/local/bin/
LOG_DIR     := /var/log/$(TARGET)/
DATA_DIR    := /var/lib/$(TARGET)/
LOCK_DIR    := /var/run/$(TARGET)/

INFO_LOG_FILE  := info_log
ERROR_LOG_FILE := error_log
USERS_FILE     := users.json

OBJ_FILES := $(patsubst $(SRC_DIR)%.c, $(BUILD_DIR)%.o, $(wildcard $(SRC_DIR)*.c))

build: $(BUILD_DIR) $(BUILD_DIR)$(TARGET)

$(BUILD_DIR):
	@echo -e '\e[0;33;1mBuilding $(TARGET)...\e[0m'

	mkdir -p $@

$(BUILD_DIR)$(TARGET): $(OBJ_FILES)
	$(CC) $^ -o $@ $(LDFLAGS)

	@echo -e "\e[0;32;1mBuilding done!\n$$($(BUILD_DIR)$(TARGET) -v) on $$(uname -mo)\e[0m"

$(BUILD_DIR)%.o: $(SRC_DIR)%.c
	$(CC) -c $< -o $@ $(CFLAGS)

-include $(OBJ_FILES:.o=.d)

clean:
	@echo -e '\e[0;33;1mCleaning $(TARGET) build files...\e[0m'

	rm -rf $(BUILD_DIR)

	@echo -e '\e[0;32;1mCleaning done!\e[0m'

install:
	@echo -e '\e[0;33;1mInstalling $(TARGET)...\e[0m'

	sudo cp $(BUILD_DIR)$(TARGET) $(BIN_DIR)

	@echo -e '\e[0;33;1mCreating $(TARGET) files...\e[0m'

	sudo mkdir -p $(LOG_DIR) $(DATA_DIR)
	sudo test -f $(DATA_DIR)$(USERS_FILE) || echo '{}' | sudo tee $(DATA_DIR)$(USERS_FILE) > /dev/null

	@echo -e '\e[0;33;1mCreating $(TARGET) user...\e[0m'

	id -u $(TARGET) > /dev/null 2>&1 || sudo useradd -Mrs /bin/false $(TARGET)

	@echo -e '\e[0;33;1mSetting access permissions...\e[0m'

	sudo chown -R $(TARGET):$(TARGET) $(LOG_DIR) $(DATA_DIR)
	sudo chmod 700 $(LOG_DIR) $(DATA_DIR)

	@echo -e '\e[0;33;1mChecking for systemd...\e[0m'

	which systemctl > /dev/null 2>&1 && sudo cp $(INIT_DIR)systemd/*.service $(SYSTEMD_DIR) && sudo systemctl daemon-reload || true

	@echo -e '\e[0;32;1mInstallation done!\e[0m'

uninstall:
	@echo -e '\e[0;33;1mDeleting $(TARGET)...\e[0m'

	sudo rm -f $(BIN_DIR)$(TARGET)

	@echo -e '\e[0;32;1mUninstallation done!\e[0m'

purge: uninstall
	@echo -e '\e[0;33;1mDeleting $(TARGET) files...\e[0m'

	sudo rm -rf $(LOG_DIR) $(DATA_DIR) $(LOCK_DIR)

	@echo -e '\e[0;33;1mDeleting $(TARGET) user...\e[0m'

	id -u $(TARGET) > /dev/null 2>&1 && sudo userdel $(TARGET) || true

	@echo -e '\e[0;33;1mChecking for systemd...\e[0m'

	which systemctl > /dev/null 2>&1 && sudo rm -f $(SYSTEMD_DIR)$(TARGET)*.service && sudo systemctl daemon-reload || true

	@echo -e '\e[0;32;1mPurging done!\e[0m'

.PHONY := build clean install uninstall purge
