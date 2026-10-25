#pragma once

#include <cstdlib>
#include <fstream>
#include <string>

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
     * @brief Фабричный метод, считывающий конфигурацию из переменных окружения или файла .env.
     * Сначала проверяет `BOT_TOKEN`, затем `TELEGRAM_BOT_TOKEN`, затем локальный файл `.env`.
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

        // Если переменная окружения не задана, пробуем считать из локального файла .env
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

                        if (key == "BOT_TOKEN" || key == "TELEGRAM_BOT_TOKEN") {
                            config.token = val;
                            break;
                        }
                    }
                }
            }
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
