#pragma once

#include <string>
#include <cstdlib>

/**
 * @file BotConfig.h
 * @brief Configuration data structures and environment loaders for the Telegram bot.
 */

namespace bot {

/**
 * @struct BotConfig
 * @brief Encapsulates runtime configuration settings for the Telegram bot service (Task 2.1).
 *
 * Provides parameters for network communication, polling timeouts, and logging levels,
 * along with helper methods to instantiate settings directly from the environment.
 */
struct BotConfig {
    /// Telegram Bot API token obtained from @BotFather.
    std::string token;

    /// Timeout in seconds for long-polling requests to Telegram servers.
    int longPollTimeoutSeconds{10};

    /// Flag enabling verbose operational and debugging output to standard streams.
    bool debugLogging{true};

    /**
     * @brief Factory method that populates configuration from standard environment variables.
     * Checks `BOT_TOKEN` first, then falls back to `TELEGRAM_BOT_TOKEN`.
     * @return Initialized BotConfig instance.
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
     * @brief Checks whether the configuration possesses all required credentials to run.
     * @return True if a non-empty API token is provided, false otherwise.
     */
    [[nodiscard]] bool isValid() const {
        return !token.empty();
    }
};

} // namespace bot
