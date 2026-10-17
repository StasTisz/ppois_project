#pragma once

#include "bot/BotEventRouter.h"
#include "bot/IBotMessenger.h"

#include <tgbot/tgbot.h>
#include <atomic>
#include <memory>
#include <string>

/**
 * @file TelegramBotApp.h
 * @brief Top-level application runner and adapter integrating the tgbot-cpp library with our event architecture.
 */

namespace bot {

/**
 * @class TgBotMessenger
 * @brief Concrete adapter mapping `IBotMessenger` interface calls to `TgBot::Bot` API operations.
 *
 * Implements fault-tolerant message dispatching and callback query answering,
 * catching and logging library-specific `TgBot::TgException` and general runtime exceptions.
 */
class TgBotMessenger : public IBotMessenger {
public:
    /**
     * @brief Constructs the adapter wrapping a reference to an active `TgBot::Bot` instance.
     * @param bot Reference to the underlying Telegram bot client.
     */
    explicit TgBotMessenger(TgBot::Bot& bot);

    /**
     * @brief Sends text message via `TgBot::Api::sendMessage`.
     * @param chatId Telegram chat ID destination.
     * @param text Message body string.
     */
    void sendMessage(int64_t chatId, const std::string& text) override;

    /**
     * @brief Acknowledges inline button callback query via `TgBot::Api::answerCallbackQuery`.
     * @param callbackId Unique Telegram callback identifier.
     * @param text Optional notification banner text.
     */
    void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") override;

private:
    TgBot::Bot& bot_;
};

/**
 * @class TelegramBotApp
 * @brief Primary application host managing bot initialization, event wire-up, and long-polling lifecycle.
 *
 * Encapsulates the long-polling loop, automatically maps `TgBot::Message` and `TgBot::CallbackQuery`
 * instances into decoupled domain DTOs (`IncomingMessage`, `IncomingCallback`), and passes them
 * to `BotEventRouter`.
 */
class TelegramBotApp {
public:
    /**
     * @brief Initializes bot network client, messenger adapter, and event router.
     * @param token Authentication token issued by @BotFather.
     */
    explicit TelegramBotApp(const std::string& token);

    /**
     * @brief Destructor ensuring polling loop termination and clean resource release.
     */
    ~TelegramBotApp();

    /**
     * @brief Executes the long-polling loop.
     *
     * Clears conflicting webhooks, logs bot identity, and enters an exception-resilient
     * polling loop using `TgBot::TgLongPoll`. Recovers gracefully from transient network drops.
     */
    void run();

    /**
     * @brief Signals the long-polling loop to terminate gracefully.
     */
    void stop();

    /**
     * @brief Accessor to the internal event router for registering FSM / Command handlers.
     * @return Shared pointer to `BotEventRouter`.
     */
    [[nodiscard]] std::shared_ptr<BotEventRouter> getRouter() const;

    /**
     * @brief Direct reference access to the underlying `TgBot::Bot` instance if low-level operations are needed.
     * @return Reference to `TgBot::Bot`.
     */
    [[nodiscard]] TgBot::Bot& getBot();

private:
    /**
     * @brief Registers tgbot-cpp event listeners and links them to BotEventRouter handlers.
     */
    void setupCallbacks();

    TgBot::Bot bot_;
    std::shared_ptr<TgBotMessenger> messenger_;
    std::shared_ptr<BotEventRouter> router_;
    std::atomic<bool> isRunning_{false};
};

} // namespace bot
