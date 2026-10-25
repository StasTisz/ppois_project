#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

/**
 * @file BotConfig.h
 * @brief Структуры конфигурации и загрузчики настроек Telegram-бота из переменных окружения.
 */

namespace bot {

/**
 * @struct BotConfig
 * @brief Инкапсулирует параметры времени выполнения для сервиса Telegram-бота (Задача 2.1).
 *
 * Содержит параметры сетевого взаимодействия, таймауты опроса, уровень логирования
 * и пути к локальной базе данных SQLite и начальному датасету фильмов.
 */
struct BotConfig {
    /// Токен Telegram Bot API, полученный у @BotFather.
    std::string token;

    /// Таймаут long-polling запросов к серверам Telegram в секундах.
    int longPollTimeoutSeconds{10};

    /// Флаг включения подробного отладочного вывода в консоль.
    bool debugLogging{true};

    /// Путь к файлу базы данных SQLite.
    std::string dbPath{"data/movies.db"};

    /// Путь к начальному датасету фильмов в формате JSON.
    std::string seedJsonPath{"data/initial_movies.json"};

    /// Идентификатор служебной группы/канала для фонового прогрева и кэширования постеров Telegram.
    int64_t cacheChatId{0};

    /**
     * @brief Находит существующие пути к файлам данных при запуске из вложенных папок сборки (CLion / VS).
     */
    void resolveDataPaths() {
        namespace fs = std::filesystem;
        if (fs::exists(seedJsonPath)) {
            return;
        }

        const std::vector<std::string> prefixes = {"../", "../../", "../../../"};
        for (const auto& prefix : prefixes) {
            std::string candidateSeed = prefix + seedJsonPath;
            if (fs::exists(candidateSeed)) {
                seedJsonPath = candidateSeed;
                if (dbPath == "data/movies.db") {
                    dbPath = prefix + dbPath;
                }
                return;
            }
            std::string fallbackSeed = prefix + "data/initial_movies.json";
            if (fs::exists(fallbackSeed)) {
                seedJsonPath = fallbackSeed;
                if (dbPath == "data/movies.db") {
                    dbPath = prefix + dbPath;
                }
                return;
            }
        }
    }

    /**
     * @brief Фабричный метод, считывающий конфигурацию из переменных окружения или файла .env.
     * Проверяет `BOT_TOKEN`, `TELEGRAM_BOT_TOKEN`, `DB_PATH`, `SEED_JSON_PATH`,
     * а затем файл `.env` в текущей и родительских папках.
     * @return Инициализированный объект BotConfig с разрешенными путями к данным.
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

        const char* dbPathEnv = std::getenv("DB_PATH");
        if (dbPathEnv && !std::string(dbPathEnv).empty()) {
            config.dbPath = dbPathEnv;
        }

        const char* seedJsonPathEnv = std::getenv("SEED_JSON_PATH");
        if (seedJsonPathEnv && !std::string(seedJsonPathEnv).empty()) {
            config.seedJsonPath = seedJsonPathEnv;
        }

        const char* cacheIdEnv = std::getenv("CACHE_CHANNEL_ID");
        if (!cacheIdEnv || std::string(cacheIdEnv).empty()) {
            cacheIdEnv = std::getenv("MEDIA_CACHE_CHAT_ID");
        }
        if (cacheIdEnv && !std::string(cacheIdEnv).empty()) {
            try {
                config.cacheChatId = std::stoll(cacheIdEnv);
            } catch (...) {}
        }

        // Поиск .env файла в текущей папке или выше по дереву каталогов
        const std::vector<std::string> envCandidates = {
            ".env",
            "../.env",
            "../../.env",
            "../../../.env"
        };

        for (const auto& envPath : envCandidates) {
            if (!std::filesystem::exists(envPath)) {
                continue;
            }
            std::ifstream envFile(envPath);
            if (!envFile.is_open()) {
                continue;
            }

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
                    } else if (key == "DB_PATH" && !dbPathEnv && !val.empty()) {
                        config.dbPath = val;
                    } else if (key == "SEED_JSON_PATH" && !seedJsonPathEnv && !val.empty()) {
                        config.seedJsonPath = val;
                    } else if ((key == "CACHE_CHANNEL_ID" || key == "MEDIA_CACHE_CHAT_ID") && !cacheIdEnv && !val.empty()) {
                        try {
                            config.cacheChatId = std::stoll(val);
                        } catch (...) {}
                    }
                }
            }
            break;
        }

        config.resolveDataPaths();
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
