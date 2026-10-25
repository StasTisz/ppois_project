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
 * @brief Инкапсулирует параметры времени выполнения для сервиса Telegram-бота (Задача 2.1).
 *
 * Содержит параметры сетевого взаимодействия, таймауты опроса и уровень логирования,
 * а также вспомогательные методы для создания конфигурации напрямую из переменных окружения.
 */
struct BotConfig {
    /// Токен Telegram Bot API, полученный у @BotFather.
    std::string token;

    /// Таймаут long-polling запросов к серверам Telegram в секундах.
    int longPollTimeoutSeconds{10};

    /// Флаг включения подробного отладочного вывода в консоль.
    bool debugLogging{true};

    /**
     * @brief Фабричный метод, считывающий конфигурацию из стандартных переменных окружения.
     * Сначала проверяет `BOT_TOKEN`, затем `TELEGRAM_BOT_TOKEN`.
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
        return config;
    }

    /**
     * @brief Проверяет наличие всех обязательных параметров для запуска бота.
     * @return true, если задан непустой токен API, иначе false.
     */
    [[nodiscard]] bool isValid() const {
        return !token.empty();
    }
};

} // namespace bot
