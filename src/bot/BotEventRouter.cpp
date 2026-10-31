#include "bot/BotEventRouter.h"
#include "bot/BotConstants.h"

#include <iostream>
#include <utility>

/**
 * @file BotEventRouter.cpp
 * @brief Реализация логики маршрутизации сообщений и callback-запросов класса BotEventRouter.
 */

namespace bot {

BotEventRouter::BotEventRouter(std::shared_ptr<IBotMessenger> messenger)
    : messenger_(std::move(messenger)) {
}

void BotEventRouter::onTextMessage(const IncomingMessage& msg) {
    if (msg.text.empty()) {
        return;
    }
    if (!messenger_) {
        std::cerr << "[BotEventRouter] Ошибка: messenger не инициализирован (null)\n";
        return;
    }
    if (customMessageHandler_) {
        customMessageHandler_(msg);
        return;
    }
    if (msg.text == "/start") {
        messenger_->sendMessage(msg.chatId, "Welcome to Movie Recommendation Bot!");
        return;
    }
    messenger_->sendMessage(msg.chatId, "Received: " + msg.text);
}

bool BotEventRouter::isDuplicateCallback(int64_t userId, int32_t messageId, const std::string& data) {
    if (userId == 0) {
        return false;
    }
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(debounceMutex_);
    auto it = userLastCallbacks_.find(userId);
    if (it != userLastCallbacks_.end() && it->second.messageId == messageId && it->second.data == data) {
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.timestamp).count();
        if (elapsedMs < kDebounceTimeoutMs) {
            return true;
        }
    }
    userLastCallbacks_[userId] = {messageId, data, now};
    return false;
}

void BotEventRouter::onCallbackQuery(const IncomingCallback& cb) {
    if (!messenger_) {
        std::cerr << "[BotEventRouter] Ошибка: messenger не инициализирован (null)\n";
        return;
    }
    messenger_->answerCallbackQuery(cb.callbackId);
    if (isDuplicateCallback(cb.userId, cb.messageId, cb.data)) {
        return;
    }
    if (customCallbackHandler_) {
        customCallbackHandler_(cb);
        return;
    }
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
