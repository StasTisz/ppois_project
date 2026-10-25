#pragma once

#include "bot/BotConfig.h"
#include "bot/BotEventRouter.h"
#include "bot/BotStateMachine.h"
#include "bot/IBotMessenger.h"
#include "bot/NetworkErrorHandler.h"
#include "recommendation/RecommendationService.h"
#include "repository/IUserRepository.h"
#include "bot/ThreadPool.h"

#include <tgbot/tgbot.h>
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

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
     * @brief Редактирует существующее текстовое сообщение с inline-клавиатурой через `TgBot::Api::editMessageText`.
     * @param chatId Идентификатор целевого чата Telegram.
     * @param messageId Идентификатор редактируемого сообщения.
     * @param text Новый текст сообщения.
     * @param keyboard Новая структура кнопок inline-клавиатуры.
     */
    void editMessageWithKeyboard(
        int64_t chatId,
        int32_t messageId,
        const std::string& text,
        const InlineKeyboard& keyboard) override;

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

    /**
     * @brief Сохраняет ассоциацию photoUrl -> telegram_file_id в локальный кэш.
     * @param photoUrl URL постера.
     * @param fileId Идентификатор файла на серверах Telegram.
     */
    void cachePhotoFileId(const std::string& photoUrl, const std::string& fileId);

    /**
     * @brief Возвращает закэшированный telegram_file_id для постера, если он уже известен.
     * @param photoUrl URL постера.
     * @return file_id либо пустая строка при отсутствии в кэше.
     */
    [[nodiscard]] std::string getCachedPhotoFileId(const std::string& photoUrl) const;

private:
    [[nodiscard]] TgBot::InlineKeyboardMarkup::Ptr buildMarkup(const InlineKeyboard& keyboard) const;

    TgBot::Bot& bot_;
    RetryConfig retryConfig_;
    std::unordered_map<std::string, std::string> photoFileIdCache_;
};

/**
 * @class TelegramBotApp
 * @brief Основной класс приложения бота с асинхронным пулом рабочих потоков.
 */
class TelegramBotApp {
public:
    explicit TelegramBotApp(const BotConfig& config);
    explicit TelegramBotApp(const std::string& token);

    /**
     * @brief Конструктор приложения с сервисом рекомендаций и репозиторием пользователей.
     * @param config Настройки приложения и токен Telegram-бота.
     * @param recService Сервис-фасад рекомендаций фильмов.
     * @param userRepo Репозиторий пользователей для синхронизации в SQLite.
     */
    TelegramBotApp(
        const BotConfig& config,
        std::shared_ptr<recommendation::RecommendationService> recService,
        std::shared_ptr<repository::IUserRepository> userRepo = nullptr);

    ~TelegramBotApp();

    void run();
    void stop();

    [[nodiscard]] std::shared_ptr<BotEventRouter> getRouter() const;
    [[nodiscard]] std::shared_ptr<BotStateMachine> getStateMachine() const;
    [[nodiscard]] std::shared_ptr<TgBotMessenger> getMessenger() const;
    [[nodiscard]] TgBot::Bot& getBot();

private:
    /**
     * @brief Запускает асинхронный фоновый прогрев постеров в служебной группе/канале Telegram.
     */
    void startMediaCacheWarmup();

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
