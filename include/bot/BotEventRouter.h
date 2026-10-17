#pragma once

#include "bot/BotEvents.h"
#include "bot/IBotMessenger.h"

#include <functional>
#include <memory>
#include <string>

namespace bot {

/**
 * @brief Routes incoming text messages and callback queries.
 * Validates incoming events and delegates them to registered handlers or standard replies.
 */
class BotEventRouter {
public:
    using MessageHandler = std::function<void(const IncomingMessage&)>;
    using CallbackHandler = std::function<void(const IncomingCallback&)>;

    explicit BotEventRouter(std::shared_ptr<IBotMessenger> messenger);

    /**
     * @brief Handles an incoming user text message.
     * Dispatches commands or forwards to custom message handler.
     */
    void onTextMessage(const IncomingMessage& msg);

    /**
     * @brief Handles an incoming callback query from inline buttons.
     * Automatically acknowledges the query to release Telegram client loader.
     */
    void onCallbackQuery(const IncomingCallback& cb);

    /**
     * @brief Registers custom handler for text messages (used by FSM / Command Dispatcher).
     */
    void setMessageHandler(MessageHandler handler);

    /**
     * @brief Registers custom handler for callback queries (used by FSM / View Factory).
     */
    void setCallbackHandler(CallbackHandler handler);

    /**
     * @brief Returns current messenger instance.
     */
    [[nodiscard]] std::shared_ptr<IBotMessenger> getMessenger() const;

private:
    std::shared_ptr<IBotMessenger> messenger_;
    MessageHandler customMessageHandler_;
    CallbackHandler customCallbackHandler_;
};

} // namespace bot
