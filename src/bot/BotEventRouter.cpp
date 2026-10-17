#include "bot/BotEventRouter.h"

#include <iostream>
#include <utility>

namespace bot {

BotEventRouter::BotEventRouter(std::shared_ptr<IBotMessenger> messenger)
    : messenger_(std::move(messenger)) {
}

void BotEventRouter::onTextMessage(const IncomingMessage& msg) {
    if (msg.text.empty()) {
        return;
    }

    if (!messenger_) {
        std::cerr << "[BotEventRouter] Error: messenger is null\n";
        return;
    }

    // Command handling (/start)
    if (msg.text == "/start") {
        messenger_->sendMessage(msg.chatId, "Welcome to Movie Recommendation Bot!");
        return;
    }

    // Forward to custom handler (e.g. FSM / Command Dispatcher in Milestone 2)
    if (customMessageHandler_) {
        customMessageHandler_(msg);
        return;
    }

    // Default fallback echo response for unhandled non-empty text
    messenger_->sendMessage(msg.chatId, "Received: " + msg.text);
}

void BotEventRouter::onCallbackQuery(const IncomingCallback& cb) {
    if (!messenger_) {
        std::cerr << "[BotEventRouter] Error: messenger is null\n";
        return;
    }

    // Always acknowledge callback query to dismiss loader spinner in Telegram client
    messenger_->answerCallbackQuery(cb.callbackId);

    // Forward to custom handler if registered
    if (customCallbackHandler_) {
        customCallbackHandler_(cb);
        return;
    }

    // Default acknowledgement feedback if no custom handler is set
    messenger_->sendMessage(cb.chatId, "Acknowledged callback: " + cb.data);
}

void BotEventRouter::setMessageHandler(MessageHandler handler) {
    customMessageHandler_ = std::move(handler);
}

void BotEventRouter::setCallbackHandler(CallbackHandler handler) {
    customCallbackHandler_ = std::move(handler);
}

std::shared_ptr<IBotMessenger> BotEventRouter::getMessenger() const {
    return messenger_;
}

} // namespace bot
