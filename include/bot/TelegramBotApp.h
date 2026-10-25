#pragma once

#include "bot/BotConfig.h"
#include "bot/BotEventRouter.h"
#include "bot/BotStateMachine.h"
#include "bot/IBotMessenger.h"
#include "bot/NetworkErrorHandler.h"
#include "bot/ThreadPool.h"

#include <tgbot/tgbot.h>
#include <atomic>
#include <memory>
#include <string>

/**
 * @file TelegramBotApp.h
 * @brief Хост приложения Telegram-бота и адаптер библиотеки tgbot-cpp.
 */

namespace bot {

/**
 * @class TgBotMessenger
 * @brief Адаптер, реализующий интерфейс `IBotMessenger` через вызовы Telegram Bot API (`tgbot-cpp`).
 */
class TgBotMessenger : public IBotMessenger {
public:
    explicit TgBotMessenger(TgBot::Bot& bot, RetryConfig retryConfig = {});

    void sendMessage(int64_t chatId, const std::string& text) override;

    void sendMessageWithKeyboard(int64_t chatId, const std::string& text, const InlineKeyboard& keyboard) override;

    /**
     * @brief Отправляет фото (постер) с подписью и inline-клавиатурой через `TgBot::Api::sendPhoto`.
     * Принимает как URL, так и Telegram file_id. Опционально возвращает file_id загруженного фото.
     */
    using IBotMessenger::sendPhotoWithKeyboard;
    bool sendPhotoWithKeyboard(
        int64_t chatId,
        const std::string& photoUrlOrFileId,
        const std::string& caption,
        const InlineKeyboard& keyboard,
        std::string* fileIdOut = nullptr) override;

    void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") override;

    [[nodiscard]] const RetryConfig& getRetryConfig() const;
    void setRetryConfig(const RetryConfig& config);

private:
    [[nodiscard]] TgBot::InlineKeyboardMarkup::Ptr buildMarkup(const InlineKeyboard& keyboard) const;

    TgBot::Bot& bot_;
    RetryConfig retryConfig_;
};

/**
 * @class TelegramBotApp
 * @brief Основной класс приложения бота с асинхронным пулом рабочих потоков.
 */
class TelegramBotApp {
public:
    explicit TelegramBotApp(const BotConfig& config);
    explicit TelegramBotApp(const std::string& token);
    ~TelegramBotApp();

    void run();
    void stop();

    [[nodiscard]] std::shared_ptr<BotEventRouter> getRouter() const;
    [[nodiscard]] std::shared_ptr<BotStateMachine> getStateMachine() const;
    [[nodiscard]] std::shared_ptr<TgBotMessenger> getMessenger() const;
    [[nodiscard]] TgBot::Bot& getBot();

private:
    void setupCallbacks();

    BotConfig config_;
    TgBot::Bot bot_;
    std::shared_ptr<TgBotMessenger> messenger_;
    std::shared_ptr<BotEventRouter> router_;
    std::shared_ptr<BotStateMachine> stateMachine_;
    std::atomic<bool> isRunning_{false};
    ThreadPool threadPool_; // Асинхронный неблокирующий пул потоков
};

} // namespace bot
