#pragma once

#include "bot/BotConfig.h"
#include "bot/BotEventRouter.h"
#include "bot/BotStateMachine.h"
#include "bot/IBotMessenger.h"
#include "bot/NetworkErrorHandler.h"
#include "recommendation/RecommendationService.h"
#include "repository/IUserRepository.h"

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
 *
 * Перехватывает и логирует библиотечные исключения `TgBot::TgException` и стандартные ошибки,
 * предотвращая аварийное завершение процесса при сетевых неполадках Telegram.
 */
class TgBotMessenger : public IBotMessenger {
public:
    /**
     * @brief Конструктор адаптера, принимающий ссылку на экземпляр `TgBot::Bot` и конфигурацию повторов.
     * @param bot Ссылка на активный клиент Telegram-бота.
     * @param retryConfig Параметры политики повторных попыток при сетевых сбоях.
     */
    explicit TgBotMessenger(TgBot::Bot& bot, RetryConfig retryConfig = {});

    /**
     * @brief Отправляет текстовое сообщение через `TgBot::Api::sendMessage`.
     * @param chatId Идентификатор целевого чата Telegram.
     * @param text Текст сообщения.
     */
    void sendMessage(int64_t chatId, const std::string& text) override;

    /**
     * @brief Отправляет текстовое сообщение с inline-клавиатурой через `TgBot::Api::sendMessage`.
     * @param chatId Идентификатор целевого чата Telegram.
     * @param text Текст сообщения.
     * @param keyboard Структура кнопок inline-клавиатуры.
     */
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
     * При сбое загрузки постера выполняет fallback на текстовое сообщение.
     * @param chatId Идентификатор целевого чата Telegram.
     * @param photoUrl URL-ссылка на изображение постера.
     * @param caption Текстовая подпись к фото.
     * @param keyboard Структура кнопок inline-клавиатуры.
     * @return true при успешной отправке фото, false если сработал fallback на текст.
     */
    bool sendPhotoWithKeyboard(
        int64_t chatId,
        const std::string& photoUrl,
        const std::string& caption,
        const InlineKeyboard& keyboard) override;

    /**
     * @brief Подтверждает callback-запрос через `TgBot::Api::answerCallbackQuery`.
     * @param callbackId Идентификатор callback-запроса Telegram.
     * @param text Опциональный текст уведомления.
     */
    void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") override;

    /**
     * @brief Возвращает текущую конфигурацию повторных попыток.
     * @return Константная ссылка на RetryConfig.
     */
    [[nodiscard]] const RetryConfig& getRetryConfig() const;

    /**
     * @brief Задает конфигурацию повторных попыток сети.
     * @param config Новые параметры RetryConfig.
     */
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
    /**
     * @brief Преобразует InlineKeyboard в платформенный объект разметки tgbot-cpp.
     * @param keyboard Структура кнопок.
     * @return Умный указатель на разметку клавиатуры.
     */
    [[nodiscard]] TgBot::InlineKeyboardMarkup::Ptr buildMarkup(const InlineKeyboard& keyboard) const;

    TgBot::Bot& bot_;
    RetryConfig retryConfig_;
    std::unordered_map<std::string, std::string> photoFileIdCache_;
};

/**
 * @class TelegramBotApp
 * @brief Основной класс приложения бота, управляющий long-polling циклом и событиями.
 *
 * Преобразует платформенные объекты сообщений (`TgBot::Message`) и callback-запросов
 * (`TgBot::CallbackQuery`) в независимые структуры DTO (`IncomingMessage`, `IncomingCallback`)
 * и направляет их в `BotEventRouter`.
 */
class TelegramBotApp {
public:
    /**
     * @brief Конструктор приложения с полной конфигурацией.
     * @param config Параметры конфигурации бота.
     */
    explicit TelegramBotApp(const BotConfig& config);

    /**
     * @brief Конструктор приложения по токену (обратная совместимость).
     * @param token Токен Telegram-бота.
     */
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

    /**
     * @brief Деструктор, останавливающий цикл опроса и освобождающий ресурсы.
     */
    ~TelegramBotApp();

    /**
     * @brief Запускает цикл опроса серверов Telegram (long-polling).
     *
     * Удаляет активные вебхуки, выводит юзернейм бота и циклически вызывает `TgBot::TgLongPoll::start()`
     * с защитой от сетевых ошибок и сбоев соединения.
     */
    void run();

    /**
     * @brief Завершает цикл long-polling.
     */
    void stop();

    /**
     * @brief Предоставляет доступ к маршрутизатору событий для регистрации обработчиков диалогов.
     * @return Умный указатель на `BotEventRouter`.
     */
    [[nodiscard]] std::shared_ptr<BotEventRouter> getRouter() const;

    /**
     * @brief Предоставляет доступ к конечному автомату диалогов.
     * @return Умный указатель на `BotStateMachine`.
     */
    [[nodiscard]] std::shared_ptr<BotStateMachine> getStateMachine() const;

    /**
     * @brief Предоставляет доступ к клиенту отправки сообщений Telegram.
     * @return Умный указатель на `TgBotMessenger`.
     */
    [[nodiscard]] std::shared_ptr<TgBotMessenger> getMessenger() const;

    /**
     * @brief Предоставляет доступ к низкоуровневому объекту `TgBot::Bot`.
     * @return Ссылка на экземпляр `TgBot::Bot`.
     */
    [[nodiscard]] TgBot::Bot& getBot();

private:
    /**
     * @brief Запускает асинхронный фоновый прогрев постеров в служебной группе/канале Telegram.
     */
    void startMediaCacheWarmup();

    /**
     * @brief Подписывается на события tgbot-cpp и связывает их с маршрутизатором `BotEventRouter`.
     */
    void setupCallbacks();

    BotConfig config_;
    TgBot::Bot bot_;
    std::shared_ptr<TgBotMessenger> messenger_;
    std::shared_ptr<BotEventRouter> router_;
    std::shared_ptr<BotStateMachine> stateMachine_;
    std::atomic<bool> isRunning_{false};
};

} // namespace bot
