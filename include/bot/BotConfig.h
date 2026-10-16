#pragma once

#include <string>
#include <cstdlib>

namespace bot {

/**
 * @brief Configuration settings for the Telegram bot infrastructure (Task 2.1).
 */
struct BotConfig {
    std::string token;
    int longPollTimeoutSeconds{10};
    bool debugLogging{true};

    /**
     * @brief Loads configuration from environment variables.
     */
    static BotConfig fromEnv() {
        BotConfig config;
        const char* tokenEnv = std::getenv("TELEGRAM_BOT_TOKEN");
        if (tokenEnv) {
            config.token = tokenEnv;
        }
        return config;
    }

    [[nodiscard]] bool isValid() const {
        return !token.empty();
    }
};

} // namespace bot
