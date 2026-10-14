#include <iostream>
#include <string>
#include <cstdlib>

#include "core/Movie.h"
#include "core/User.h"
#include "core/Rating.h"
#include "bot/BotState.h"

int main() {
    std::cout << "========================================\n";
    std::cout << " Movie Recommendation Telegram Bot (C++)\n";
    std::cout << " Project Core Initialized (Milestone 0) \n";
    std::cout << "========================================\n\n";

    const char* tokenEnv = std::getenv("TELEGRAM_BOT_TOKEN");
    if (!tokenEnv || std::string(tokenEnv).empty()) {
        std::cout << "[INFO] Variable TELEGRAM_BOT_TOKEN is not set yet.\n"
                  << "       Developer 2 will connect tgbot-cpp polling loop.\n";
    } else {
        std::cout << "[OK] Found TELEGRAM_BOT_TOKEN in environment.\n";
    }

    std::cout << "[INFO] Ready for Dev 1 (Database & Recommendations) and Dev 2 (Telegram Bot & FSM) modules.\n";
    return 0;
}
