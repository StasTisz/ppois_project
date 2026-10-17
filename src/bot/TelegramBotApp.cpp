#include "bot/TelegramBotApp.h"

#include <iostream>

/**
 * @file TelegramBotApp.cpp
 * @brief Implementation of TelegramBotApp runtime polling loop and TgBotMessenger adapter.
 */

namespace bot {

// --- TgBotMessenger Implementation ---

/**
 * @brief Constructs the messenger adapter.
 * @param bot Reference to the parent `TgBot::Bot` instance.
 */
TgBotMessenger::TgBotMessenger(TgBot::Bot& bot)
    : bot_(bot) {
}

/**
 * @brief Sends an outbound text message to the specified Telegram chat.
 * Catches Telegram API and standard runtime exceptions to prevent process termination.
 * @param chatId Destination chat identifier.
 * @param text Content string.
 */
void TgBotMessenger::sendMessage(int64_t chatId, const std::string& text) {
    try {
        bot_.getApi().sendMessage(chatId, text);
    } catch (const TgBot::TgException& e) {
        std::cerr << "[TgBotMessenger] Telegram API error in sendMessage: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[TgBotMessenger] Unexpected error in sendMessage: " << e.what() << "\n";
    }
}

/**
 * @brief Sends callback query acknowledgement to Telegram servers.
 * @param callbackId Target callback query identifier.
 * @param text Optional text to display as notification.
 */
void TgBotMessenger::answerCallbackQuery(const std::string& callbackId, const std::string& text) {
    try {
        bot_.getApi().answerCallbackQuery(callbackId, text);
    } catch (const TgBot::TgException& e) {
        std::cerr << "[TgBotMessenger] Telegram API error in answerCallbackQuery: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[TgBotMessenger] Unexpected error in answerCallbackQuery: " << e.what() << "\n";
    }
}

// --- TelegramBotApp Implementation ---

/**
 * @brief Constructs the bot application, wiring up the messenger adapter and router.
 * @param token Authentication token string.
 */
TelegramBotApp::TelegramBotApp(const std::string& token)
    : bot_(token),
      messenger_(std::make_shared<TgBotMessenger>(bot_)),
      router_(std::make_shared<BotEventRouter>(messenger_)) {
    setupCallbacks();
}

/**
 * @brief Destructor stopping background activities.
 */
TelegramBotApp::~TelegramBotApp() {
    stop();
}

/**
 * @brief Registers tgbot-cpp event listeners and adapts incoming library objects into DTOs.
 */
void TelegramBotApp::setupCallbacks() {
    bot_.getEvents().onAnyMessage([this](const TgBot::Message::Ptr& message) {
        if (!message) {
            return;
        }

        IncomingMessage msg;
        if (message->chat) {
            msg.chatId = message->chat->id;
        }
        if (message->from) {
            msg.userId = message->from->id;
            msg.username = message->from->username;
        }
        msg.text = message->text;

        router_->onTextMessage(msg);
    });

    bot_.getEvents().onCallbackQuery([this](const TgBot::CallbackQuery::Ptr& query) {
        if (!query) {
            return;
        }

        IncomingCallback cb;
        cb.callbackId = query->id;
        if (query->message && query->message->chat) {
            cb.chatId = query->message->chat->id;
        }
        if (query->from) {
            cb.userId = query->from->id;
        }
        if (query->message) {
            cb.messageId = query->message->messageId;
        }
        cb.data = query->data;

        router_->onCallbackQuery(cb);
    });
}

/**
 * @brief Initiates long-polling loop against Telegram Bot API.
 * Cleans webhooks on entry and loops `TgBot::TgLongPoll::start()` with comprehensive error handling.
 */
void TelegramBotApp::run() {
    isRunning_ = true;

    try {
        bot_.getApi().deleteWebhook();
    } catch (const TgBot::TgException& e) {
        std::cerr << "[TelegramBotApp] Notice: deleteWebhook call: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[TelegramBotApp] Error during deleteWebhook: " << e.what() << "\n";
    }

    try {
        std::cout << "[TelegramBotApp] Starting long-polling as @"
                  << bot_.getApi().getMe()->username << "...\n";
    } catch (const std::exception& e) {
        std::cerr << "[TelegramBotApp] Warning: could not retrieve bot info: " << e.what() << "\n";
    }

    TgBot::TgLongPoll longPoll(bot_);
    while (isRunning_) {
        try {
            longPoll.start();
        } catch (const TgBot::TgException& e) {
            std::cerr << "[TelegramBotApp] Telegram API exception: " << e.what() << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[TelegramBotApp] Standard exception: " << e.what() << "\n";
        }
    }

    std::cout << "[TelegramBotApp] Long-polling stopped.\n";
}

/**
 * @brief Signals termination of the active polling loop.
 */
void TelegramBotApp::stop() {
    isRunning_ = false;
}

/**
 * @brief Provides access to the event router.
 * @return Shared pointer to `BotEventRouter`.
 */
std::shared_ptr<BotEventRouter> TelegramBotApp::getRouter() const {
    return router_;
}

/**
 * @brief Provides access to the inner `TgBot::Bot` instance.
 * @return Reference to `TgBot::Bot`.
 */
TgBot::Bot& TelegramBotApp::getBot() {
    return bot_;
}

} // namespace bot
