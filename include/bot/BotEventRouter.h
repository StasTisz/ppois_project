#pragma once

#include "bot/BotEvents.h"
#include "bot/IBotMessenger.h"

#include <functional>
#include <memory>
#include <string>

/**
 * @file BotEventRouter.h
 * @brief Маршрутизатор входящих текстовых сообщений и callback-запросов Telegram.
 */

namespace bot {

/**
 * @class BotEventRouter
 * @brief Центральный диспетчер событий для клиентского слоя бота.
 *
 * Принимает входящие события от сетевого адаптера, выполняет валидацию,
 * обрабатывает базовые команды (например, `/start`) и делегирует сообщения
 * зарегистрированным обработчикам (конечному автомату диалогов FSM или диспетчеру команд).
 */
class BotEventRouter {
public:
    /// Сигнатура обработчика входящих текстовых сообщений.
    using MessageHandler = std::function<void(const IncomingMessage&)>;

    /// Сигнатура обработчика входящих callback-запросов от кнопок.
    using CallbackHandler = std::function<void(const IncomingCallback&)>;

    /**
     * @brief Создает маршрутизатор, связанный с указанным мессенджером.
     * @param messenger Умный указатель на реализацию `IBotMessenger`.
     */
    explicit BotEventRouter(std::shared_ptr<IBotMessenger> messenger);

    /**
     * @brief Обрабатывает входящее текстовое сообщение.
     *
     * Игнорирует пустые сообщения, проверяет системные команды (например, `/start`),
     * а при их отсутствии передает сообщение в зарегистрированный `MessageHandler`.
     *
     * @param msg DTO входящего сообщения.
     */
    void onTextMessage(const IncomingMessage& msg);

    /**
     * @brief Обрабатывает входящий callback-запрос от inline-кнопки.
     *
     * Гарантированно подтверждает запрос через `IBotMessenger::answerCallbackQuery`
     * для снятия индикатора ожидания в клиенте, после чего передает данные в `CallbackHandler`.
     *
     * @param cb DTO входящего callback-запроса.
     */
    void onCallbackQuery(const IncomingCallback& cb);

    /**
     * @brief Регистрирует пользовательский обработчик текстовых сообщений (FSM или диспетчер команд).
     * @param handler Функция-обработчик входящих сообщений.
     */
    void setMessageHandler(MessageHandler handler);

    /**
     * @brief Регистрирует пользовательский обработчик callback-запросов от кнопок.
     * @param handler Функция-обработчик нажатий кнопок.
     */
    void setCallbackHandler(CallbackHandler handler);

    /**
     * @brief Возвращает используемый экземпляр мессенджера.
     * @return Умный указатель на `IBotMessenger`.
     */
    [[nodiscard]] std::shared_ptr<IBotMessenger> getMessenger() const;

private:
    std::shared_ptr<IBotMessenger> messenger_;
    MessageHandler customMessageHandler_;
    CallbackHandler customCallbackHandler_;
};

} // namespace bot
