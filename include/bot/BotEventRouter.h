#pragma once

#include "bot/BotEvents.h"
#include "bot/IBotMessenger.h"

#include <functional>
#include <memory>
#include <string>

/**
 * @file BotEventRouter.h
 * @brief Dispatcher and validation layer for incoming Telegram user messages and callback queries.
 */

namespace bot {

/**
 * @class BotEventRouter
 * @brief Central event dispatching router for the bot presentation layer.
 *
 * Receives raw incoming events from network adapters, performs initial message validation,
 * handles default commands (such as `/start`), and dispatches domain payloads to registered
 * consumers (e.g., Finite State Machine dialogue handlers or command dispatchers).
 */
class BotEventRouter {
public:
    /// Callback signature for consumers processing incoming text messages.
    using MessageHandler = std::function<void(const IncomingMessage&)>;

    /// Callback signature for consumers processing inline button callbacks.
    using CallbackHandler = std::function<void(const IncomingCallback&)>;

    /**
     * @brief Constructs an event router bound to a designated messaging service.
     * @param messenger Shared pointer to an implementation of `IBotMessenger`.
     */
    explicit BotEventRouter(std::shared_ptr<IBotMessenger> messenger);

    /**
     * @brief Processes an incoming text message.
     *
     * Validates message contents, discards empty messages, checks for predefined commands
     * (e.g. `/start`), and forwards non-command text to the registered message handler.
     *
     * @param msg Incoming message DTO containing sender and text details.
     */
    void onTextMessage(const IncomingMessage& msg);

    /**
     * @brief Processes an incoming inline callback query.
     *
     * Automatically acknowledges the query via `IBotMessenger::answerCallbackQuery`
     * to eliminate client UI spinner latency, then forwards the event to the custom callback handler.
     *
     * @param cb Incoming callback DTO with button payload and origin chat metadata.
     */
    void onCallbackQuery(const IncomingCallback& cb);

    /**
     * @brief Injects a custom message handler callback (e.g. FSM or Command Router).
     * @param handler Function or callable to invoke on incoming text messages.
     */
    void setMessageHandler(MessageHandler handler);

    /**
     * @brief Injects a custom callback query handler (e.g. Inline keyboard interaction logic).
     * @param handler Function or callable to invoke on incoming callback queries.
     */
    void setCallbackHandler(CallbackHandler handler);

    /**
     * @brief Accessor for the underlying outbound messaging client.
     * @return Shared pointer to `IBotMessenger`.
     */
    [[nodiscard]] std::shared_ptr<IBotMessenger> getMessenger() const;

private:
    std::shared_ptr<IBotMessenger> messenger_;
    MessageHandler customMessageHandler_;
    CallbackHandler customCallbackHandler_;
};

} // namespace bot
