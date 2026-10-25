#pragma once

#include <string>
#include <cstdlib>

/**
 * @file BotConfig.h
 * @brief Структуры конфигурации и загрузчики настроек Telegram-бота из переменных окружения.
 */

namespace bot {

/**
 * @struct BotConfig
 * @brief Инкапсулирует параметры времени выполнения для сервиса Telegram-бота.
 */
struct BotConfig {
    /// Токен Telegram Bot API, полученный у @BotFather.
    std::string token;

    /// Таймаут long-polling запросов к серверам Telegram в секундах.
    int longPollTimeoutSeconds{10};

    /// Флаг включения подробного отладочного вывода в консоль.
    bool debugLogging{true};

    /// Опциональный chat_id для предварительного кэширования медиа / хранения постеров.
    int64_t storageChatId{0};

    /// Размер пула потоков для асинхронной неблокирующей обработки событий.
    size_t threadPoolSize{4};

    /**
     * @brief Фабричный метод, считывающий конфигурацию из переменных окружения.
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

        return config;
    }

    [[nodiscard]] bool isValid() const {
        return !token.empty();
    }
};

} // namespace bot
