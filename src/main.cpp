#include <iostream>
#include <string>
#include <cstdlib>

#include "bot/TelegramBotApp.h"

/**
 * @file main.cpp
 * @brief Application entry point for the Movie Recommendation Telegram Bot.
 *
 * Checks environment credentials (`BOT_TOKEN` or `TELEGRAM_BOT_TOKEN`),
 * instantiates the Telegram bot application host, and initiates the polling loop.
 * If credentials are missing, exits gracefully with informational instructions.
 */

/**
 * @brief Main execution function.
 * @return 0 on normal startup and execution or graceful absence of token; 1 on fatal error.
 */
int main() {
    std::cout << "========================================\n";
    std::cout << " Movie Recommendation Telegram Bot (C++)\n";
    std::cout << " Bot & Event Infrastructure (Task 2.1) \n";
    std::cout << "========================================\n\n";

    const char* tokenEnv = std::getenv("BOT_TOKEN");
    if (!tokenEnv || std::string(tokenEnv).empty()) {
        tokenEnv = std::getenv("TELEGRAM_BOT_TOKEN");
    }

    if (!tokenEnv || std::string(tokenEnv).empty()) {
        std::cout << "[INFO] Environment variable BOT_TOKEN (or TELEGRAM_BOT_TOKEN) is not set.\n"
                  << "       To run the bot in network long-polling mode, set your token:\n"
                  << "       export BOT_TOKEN=\"<YOUR_TELEGRAM_BOT_TOKEN>\"\n"
                  << "       Exiting gracefully (exit code 0).\n";
        return 0;
    }

    std::cout << "[INFO] Initializing TelegramBotApp with provided token...\n";
    try {
        bot::TelegramBotApp app(tokenEnv);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Unhandled exception in TelegramBotApp: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
