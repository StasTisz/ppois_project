#include "bot/BotEventRouter.h"

#include <iostream>
#include <utility>

/**
 * @file BotEventRouter.cpp
 * @brief Реализация логики маршрутизации сообщений и callback-запросов класса BotEventRouter.
 */

namespace bot {

/**
 * @brief Конструктор маршрутизатора событий.
 * @param messenger Умный указатель на используемый `IBotMessenger`.
 */
BotEventRouter::BotEventRouter(std::shared_ptr<IBotMessenger> messenger)
    : messenger_(std::move(messenger)) {
}

/**
 * @brief Обработка входящего текстового сообщения.
 * Отправляет приветствие на `/start`, делегирует в кастомный хендлер либо возвращает эхо-ответ.
 * @param msg DTO входящего сообщения.
 */
void BotEventRouter::onTextMessage(const IncomingMessage& msg) {
    if (msg.text.empty()) {
        return;
    }

    if (!messenger_) {
        std::cerr << "[BotEventRouter] Ошибка: messenger не инициализирован (null)\n";
        return;
    }

    // Делегирование в зарегистрированный обработчик (FSM / Command Dispatcher), включая /start
    if (customMessageHandler_) {
        customMessageHandler_(msg);
        return;
    }

    // Базовая обработка команды онбординга (/start), если кастомный обработчик не задан
    if (msg.text == "/start") {
        messenger_->sendMessage(msg.chatId, "Welcome to Movie Recommendation Bot!");
        return;
    }

    // Базовый ответ для необработанных текстовых сообщений
    messenger_->sendMessage(msg.chatId, "Received: " + msg.text);
}

/**
 * @brief Обработка callback-запроса от inline-кнопок.
 * Вызывает answerCallbackQuery и направляет данные в зарегистрированный обработчик.
 * @param cb DTO входящего callback-запроса.
 */
void BotEventRouter::onCallbackQuery(const IncomingCallback& cb) {
    if (!messenger_) {
        std::cerr << "[BotEventRouter] Ошибка: messenger не инициализирован (null)\n";
        return;
    }

    // Защита от случайного повторного клика (debounce 400 ms)
    auto now = std::chrono::steady_clock::now();
    if (cb.userId != 0 &&
        lastCallback_.userId == cb.userId &&
        lastCallback_.messageId == cb.messageId &&
        lastCallback_.data == cb.data) {
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCallback_.timestamp).count();
        if (elapsedMs < 400) {
            // Подтверждаем callback в Telegram API и игнорируем дублирующее действие
            messenger_->answerCallbackQuery(cb.callbackId);
            return;
        }
    }
    lastCallback_ = {cb.userId, cb.messageId, cb.data, now};

    // Обязательное подтверждение запроса для прекращения анимации загрузки в Telegram
    messenger_->answerCallbackQuery(cb.callbackId);

    // Передача события в зарегистрированный обработчик кнопок
    if (customCallbackHandler_) {
        customCallbackHandler_(cb);
        return;
    }

    // Базовое уведомление, если кастомный обработчик не задан
    messenger_->sendMessage(cb.chatId, "Acknowledged callback: " + cb.data);
}

/**
 * @brief Регистрация обработчика входящих текстовых сообщений.
 * @param handler Целевая функция или лямбда.
 */
void BotEventRouter::setMessageHandler(MessageHandler handler) {
    customMessageHandler_ = std::move(handler);
}

/**
 * @brief Регистрация обработчика входящих callback-запросов от кнопок.
 * @param handler Целевая функция или лямбда.
 */
void BotEventRouter::setCallbackHandler(CallbackHandler handler) {
    customCallbackHandler_ = std::move(handler);
}

/**
 * @brief Получение указателя на связанный мессенджер.
 * @return Умный указатель на `IBotMessenger`.
 */
std::shared_ptr<IBotMessenger> BotEventRouter::getMessenger() const {
    return messenger_;
}

} // namespace bot
