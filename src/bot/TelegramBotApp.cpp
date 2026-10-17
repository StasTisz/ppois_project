#include "bot/TelegramBotApp.h"

#include <iostream>

namespace bot {

// --- TgBotMessenger Implementation ---

TgBotMessenger::TgBotMessenger(TgBot::Bot& bot)
    : bot_(bot) {
}

void TgBotMessenger::sendMessage(int64_t chatId, const std::string& text) {
    try {
        bot_.getApi().sendMessage(chatId, text);
    } catch (const TgBot::TgException& e) {
        std::cerr << "[TgBotMessenger] Telegram API error in sendMessage: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[TgBotMessenger] Unexpected error in sendMessage: " << e.what() << "\n";
    }
}

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

TelegramBotApp::TelegramBotApp(const std::string& token)
    : bot_(token),
      messenger_(std::make_shared<TgBotMessenger>(bot_)),
      router_(std::make_shared<BotEventRouter>(messenger_)) {
    setupCallbacks();
}

TelegramBotApp::~TelegramBotApp() {
    stop();
}

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

void TelegramBotApp::stop() {
    isRunning_ = false;
}

std::shared_ptr<BotEventRouter> TelegramBotApp::getRouter() const {
    return router_;
}

TgBot::Bot& TelegramBotApp::getBot() {
    return bot_;
}

} // namespace bot
