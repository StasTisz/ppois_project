#pragma once

#include <cstdlib>
#include <fstream>
#include <string>

/**
 * @file BotConfig.h
 * @brief Структура конфигурации Telegram-бота и параметры запуска.
 */

namespace bot {

/**
 * @struct BotConfig
 * @brief Параметры инициализации и сетевого окружения бота.
 */
struct BotConfig {
    /// Токен Telegram Bot API, полученный у @BotFather.
    std::string token;

    /// Таймаут опроса long-polling в секундах.
    int longPollTimeoutSeconds{60};

    /// Идентификатор канала или супергруппы для кэширования медиа-файлов (опционально).
    int64_t storageChatId{0};

    /// Флаг вывода подробных логов в консоль.
    bool debugLogging{true};

    /// Размер пула потоков для асинхронной неблокирующей обработки событий.
    size_t threadPoolSize{4};

    /**
     * @brief Фабричный метод, считывающий конфигурацию из переменных окружения или файла .env.
     * Сначала проверяет переменные окружения, затем локальный файл `.env`.
     * @return Инициализированный объект BotConfig.
     */
    static BotConfig fromEnv() {
        BotConfig config;
        const char* tokenEnv = std::getenv("BOT_TOKEN");
        if (!tokenEnv || std::string(tokenEnv).empty()) {
            tokenEnv = std::getenv("TELEGRAM_BOT_TOKEN");
        }
        if (tokenEnv) {
            config.token = tokenEnv;
        }

        const char* storageChatEnv = std::getenv("BOT_STORAGE_CHAT_ID");
        if (!storageChatEnv || std::string(storageChatEnv).empty()) {
            storageChatEnv = std::getenv("STORAGE_CHAT_ID");
        }
        if (storageChatEnv) {
            try {
                config.storageChatId = std::stoll(storageChatEnv);
            } catch (...) {
                config.storageChatId = 0;
            }
        }

        const char* threadsEnv = std::getenv("BOT_THREAD_POOL_SIZE");
        if (threadsEnv) {
            try {
                config.threadPoolSize = std::stoul(threadsEnv);
            } catch (...) {
                config.threadPoolSize = 4;
            }
        }

        // Если токен не задан в переменных окружения, пробуем считать из локального файла .env
        if (config.token.empty()) {
            std::ifstream envFile(".env");
            if (envFile.is_open()) {
                std::string line;
                while (std::getline(envFile, line)) {
                    while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
                        line.erase(line.begin());
                    }
                    while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
                        line.pop_back();
                    }
                    if (line.empty() || line[0] == '#') {
                        continue;
                    }

                    auto pos = line.find('=');
                    if (pos != std::string::npos) {
                        std::string key = line.substr(0, pos);
                        std::string val = line.substr(pos + 1);
                        while (!val.empty() && (val.front() == ' ' || val.front() == '"' || val.front() == '\'')) {
                            val.erase(val.begin());
                        }
                        while (!val.empty() && (val.back() == ' ' || val.back() == '"' || val.back() == '\'' || val.back() == '\r')) {
                            val.pop_back();
                        }

                        if ((key == "BOT_TOKEN" || key == "TELEGRAM_BOT_TOKEN") && config.token.empty()) {
                            config.token = val;
                        } else if ((key == "BOT_STORAGE_CHAT_ID" || key == "STORAGE_CHAT_ID") && config.storageChatId == 0) {
                            try {
                                config.storageChatId = std::stoll(val);
                            } catch (...) {}
                        }
                    }
                }
            }
        }

        return config;
    }

    [[nodiscard]] bool isValid() const {
        return !token.empty();
    }
};

} // namespace bot
