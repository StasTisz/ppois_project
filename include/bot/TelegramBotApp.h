#pragma once

#include "bot/BotEventRouter.h"
#include "bot/IBotMessenger.h"

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
 *
 * Перехватывает и логирует библиотечные исключения `TgBot::TgException` и стандартные ошибки,
 * предотвращая аварийное завершение процесса при сетевых неполадках Telegram.
 */
class TgBotMessenger : public IBotMessenger {
public:
    /**
     * @brief Конструктор адаптера, принимающий ссылку на экземпляр `TgBot::Bot`.
     * @param bot Ссылка на активный клиент Telegram-бота.
     */
    explicit TgBotMessenger(TgBot::Bot& bot);

    /**
     * @brief Отправляет текстовое сообщение через `TgBot::Api::sendMessage`.
     * @param chatId Идентификатор целевого чата Telegram.
     * @param text Текст сообщения.
     */
    void sendMessage(int64_t chatId, const std::string& text) override;

    /**
     * @brief Подтверждает callback-запрос через `TgBot::Api::answerCallbackQuery`.
     * @param callbackId Идентификатор callback-запроса Telegram.
     * @param text Опциональный текст уведомления.
     */
    void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") override;

private:
    TgBot::Bot& bot_;
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
     * @brief Конструктор приложения, инициализирующий сетевой клиент бота и маршрутизатор.
     * @param token Токен авторизации бота от @BotFather.
     */
    explicit TelegramBotApp(const std::string& token);

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
     * @brief Предоставляет доступ к низкоуровневому объекту `TgBot::Bot`.
     * @return Ссылка на экземпляр `TgBot::Bot`.
     */
    [[nodiscard]] TgBot::Bot& getBot();

private:
    /**
     * @brief Подписывается на события tgbot-cpp и связывает их с маршрутизатором `BotEventRouter`.
     */
    void setupCallbacks();

    TgBot::Bot bot_;
    std::shared_ptr<TgBotMessenger> messenger_;
    std::shared_ptr<BotEventRouter> router_;
    std::atomic<bool> isRunning_{false};
};

} // namespace bot
