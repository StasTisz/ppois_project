#pragma once

#include "bot/BotConstants.h"

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

namespace detail {

inline void trimString(std::string& s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.pop_back();
    }
}

inline void trimQuotes(std::string& s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '"' || s.front() == '\'')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '"' || s.back() == '\'' || s.back() == '\r')) {
        s.pop_back();
    }
}

} // namespace detail

/**
 * @struct BotConfig
 * @brief Инкапсулирует параметры времени выполнения для сервиса Telegram-бота.
 */
struct BotConfig {
    /// Токен Telegram Bot API, полученный у @BotFather.
    std::string token;

    /// Таймаут long-polling запросов к серверам Telegram в секундах.
    int longPollTimeoutSeconds{kDefaultLongPollTimeoutSeconds};

    /// Флаг включения подробного отладочного вывода в консоль.
    bool debugLogging{true};

    /// Путь к файлу базы данных SQLite.
    std::string dbPath{"data/movies.db"};

    /// Путь к начальному датасету фильмов в формате JSON.
    std::string seedJsonPath{"data/initial_movies.json"};

    /// Идентификатор служебной группы/канала для фонового прогрева и кэширования постеров Telegram.
    int64_t cacheChatId{0};

    /// Опциональный chat_id для предварительного кэширования медиа / хранения постеров.
    int64_t storageChatId{0};

    /// Размер пула потоков для асинхронной неблокирующей обработки событий.
    size_t threadPoolSize{kDefaultThreadPoolSize};

    /**
     * @brief Находит существующие пути к файлам данных при запуске из вложенных папок сборки (CLion / VS).
     */
    bool tryResolveWithPrefix(const std::string& prefix) {
        namespace fs = std::filesystem;
        for (const auto& candidate : {prefix + seedJsonPath, prefix + "data/initial_movies.json"}) {
            if (fs::exists(candidate)) {
                seedJsonPath = candidate;
                if (dbPath == "data/movies.db") {
                    dbPath = prefix + dbPath;
                }
                return true;
            }
        }
        return false;
    }

    void resolveDataPaths() {
        if (std::filesystem::exists(seedJsonPath)) {
            return;
        }
        for (const auto& prefix : {"../", "../../", "../../../"}) {
            if (tryResolveWithPrefix(prefix)) {
                return;
            }
        }
    }

    /**
     * @brief Фабричный метод, считывающий конфигурацию из переменных окружения или файла .env.
     */
    static BotConfig fromEnv() {
        BotConfig config;
        readEnvironmentVariables(config);
        readDotEnvFiles(config);
        normalizeChatIds(config);
        config.resolveDataPaths();
        return config;
    }

    [[nodiscard]] bool isValid() const {
        return !token.empty();
    }

private:
    static void readEnvironmentVariables(BotConfig& config) {
        readTokenEnv(config);
        readDbPathEnv(config);
        readStorageChatIdEnv(config);
        readThreadsEnv(config);
    }

    static void readTokenEnv(BotConfig& config) {
        const char* tokenEnv = std::getenv("BOT_TOKEN");
        if (!tokenEnv || std::string(tokenEnv).empty()) {
            tokenEnv = std::getenv("TELEGRAM_BOT_TOKEN");
        }
        if (tokenEnv) {
            config.token = tokenEnv;
        }
    }

    static void readDbPathEnv(BotConfig& config) {
        const char* dbPathEnv = std::getenv("DB_PATH");
        if (dbPathEnv && !std::string(dbPathEnv).empty()) {
            config.dbPath = dbPathEnv;
        }
        const char* seedJsonEnv = std::getenv("SEED_JSON_PATH");
        if (seedJsonEnv && !std::string(seedJsonEnv).empty()) {
            config.seedJsonPath = seedJsonEnv;
        }
    }

    static void readStorageChatIdEnv(BotConfig& config) {
        const std::vector<const char*> candidates = {
            "CACHE_CHANNEL_ID", "MEDIA_CACHE_CHAT_ID",
            "BOT_STORAGE_CHAT_ID", "STORAGE_CHAT_ID"
        };
        for (const char* name : candidates) {
            const char* val = std::getenv(name);
            if (val && !std::string(val).empty()) {
                tryParseChatId(config, val);
                break;
            }
        }
    }

    static void tryParseChatId(BotConfig& config, const std::string& val) {
        try {
            config.cacheChatId = std::stoll(val);
            config.storageChatId = config.cacheChatId;
        } catch (...) {}
    }

    static void readThreadsEnv(BotConfig& config) {
        const char* threadsEnv = std::getenv("BOT_THREAD_POOL_SIZE");
        if (threadsEnv) {
            try {
                config.threadPoolSize = std::stoul(threadsEnv);
            } catch (...) {
                config.threadPoolSize = kDefaultThreadPoolSize;
            }
        }
    }

    static void readDotEnvFiles(BotConfig& config) {
        const std::vector<std::string> envCandidates = {
            ".env", "../.env", "../../.env", "../../../.env"
        };
        for (const auto& envPath : envCandidates) {
            if (std::filesystem::exists(envPath) && loadSingleEnvFile(config, envPath)) {
                break;
            }
        }
    }

    static bool loadSingleEnvFile(BotConfig& config, const std::string& envPath) {
        std::ifstream envFile(envPath);
        if (!envFile.is_open()) {
            return false;
        }
        std::string line;
        while (std::getline(envFile, line)) {
            processDotEnvLine(config, line);
        }
        return true;
    }

    static void processDotEnvLine(BotConfig& config, std::string line) {
        detail::trimString(line);
        if (line.empty() || line[0] == '#') {
            return;
        }
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            return;
        }
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        detail::trimString(key);
        detail::trimQuotes(val);
        applyDotEnvKey(config, key, val);
    }

    static void applyDotEnvKey(BotConfig& config, const std::string& key, const std::string& val) {
        if ((key == "BOT_TOKEN" || key == "TELEGRAM_BOT_TOKEN") && config.token.empty()) {
            config.token = val;
        } else if (key == "DB_PATH" && !std::getenv("DB_PATH") && !val.empty()) {
            config.dbPath = val;
        } else if (key == "SEED_JSON_PATH" && !std::getenv("SEED_JSON_PATH") && !val.empty()) {
            config.seedJsonPath = val;
        } else if (isStorageKey(key) && config.storageChatId == 0 && !val.empty()) {
            tryParseChatId(config, val);
        } else if (key == "BOT_THREAD_POOL_SIZE" && !std::getenv("BOT_THREAD_POOL_SIZE") && !val.empty()) {
            tryParseThreads(config, val);
        }
    }

    static bool isStorageKey(const std::string& key) {
        return key == "CACHE_CHANNEL_ID" || key == "MEDIA_CACHE_CHAT_ID" ||
               key == "BOT_STORAGE_CHAT_ID" || key == "STORAGE_CHAT_ID";
    }

    static void tryParseThreads(BotConfig& config, const std::string& val) {
        try {
            config.threadPoolSize = std::stoul(val);
        } catch (...) {}
    }

    static void normalizeChatIds(BotConfig& config) {
        if (config.storageChatId != 0 && config.cacheChatId == 0) {
            config.cacheChatId = config.storageChatId;
        } else if (config.cacheChatId != 0 && config.storageChatId == 0) {
            config.storageChatId = config.cacheChatId;
        }
    }
};

} // namespace bot
