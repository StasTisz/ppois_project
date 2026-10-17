#include "bot/BotEventRouter.h"

#include <iostream>
#include <utility>

/**
 * @file BotEventRouter.cpp
 * @brief Implementation of the BotEventRouter message and callback query routing logic.
 */

namespace bot {

/**
 * @brief Constructor storing the messenger client instance.
 * @param messenger Shared pointer to an active `IBotMessenger` implementation.
 */
BotEventRouter::BotEventRouter(std::shared_ptr<IBotMessenger> messenger)
    : messenger_(std::move(messenger)) {
}

/**
 * @brief Handles text messages by checking for onboarding commands (/start),
 * falling back to custom consumers or default acknowledgment.
 * @param msg Incoming message DTO.
 */
void BotEventRouter::onTextMessage(const IncomingMessage& msg) {
    if (msg.text.empty()) {
        return;
    }

    if (!messenger_) {
        std::cerr << "[BotEventRouter] Error: messenger is null\n";
        return;
    }

    // Default command handling for onboarding (/start)
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

/**
 * @brief Processes callback queries, immediately clearing the UI wait indicator
 * and forwarding payload data to registered listeners.
 * @param cb Incoming callback query DTO.
 */
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

/**
 * @brief Injects custom text message routing callback.
 * @param handler Target callable.
 */
void BotEventRouter::setMessageHandler(MessageHandler handler) {
    customMessageHandler_ = std::move(handler);
}

/**
 * @brief Injects custom callback query routing callback.
 * @param handler Target callable.
 */
void BotEventRouter::setCallbackHandler(CallbackHandler handler) {
    customCallbackHandler_ = std::move(handler);
}

/**
 * @brief Retrieves the active messenger reference.
 * @return Shared pointer to `IBotMessenger`.
 */
std::shared_ptr<IBotMessenger> BotEventRouter::getMessenger() const {
    return messenger_;
}

} // namespace bot
