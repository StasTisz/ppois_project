#pragma once

#include "bot/BotEventRouter.h"
#include "bot/IBotMessenger.h"

#include <tgbot/tgbot.h>
#include <atomic>
#include <memory>
#include <string>

namespace bot {

/**
 * @brief Adapter implementing IBotMessenger using tgbot-cpp API.
 */
class TgBotMessenger : public IBotMessenger {
public:
    explicit TgBotMessenger(TgBot::Bot& bot);

    void sendMessage(int64_t chatId, const std::string& text) override;
    void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") override;

private:
    TgBot::Bot& bot_;
};

/**
 * @brief Telegram bot application managing tgbot-cpp event loop and lifecycle.
 */
class TelegramBotApp {
public:
    explicit TelegramBotApp(const std::string& token);
    ~TelegramBotApp();

    /**
     * @brief Starts the Telegram long-polling loop with exception resilience.
     */
    void run();

    /**
     * @brief Stops the running long-polling loop.
     */
    void stop();

    [[nodiscard]] std::shared_ptr<BotEventRouter> getRouter() const;
    [[nodiscard]] TgBot::Bot& getBot();

private:
    void setupCallbacks();

    TgBot::Bot bot_;
    std::shared_ptr<TgBotMessenger> messenger_;
    std::shared_ptr<BotEventRouter> router_;
    std::atomic<bool> isRunning_{false};
};

} // namespace bot
