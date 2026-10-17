#include "bot/TelegramBotApp.h"

#include <iostream>

/**
 * @file TelegramBotApp.cpp
 * @brief Реализация цикла long-polling приложения TelegramBotApp и адаптера TgBotMessenger.
 */

namespace bot {

// --- Реализация TgBotMessenger ---

/**
 * @brief Конструктор адаптера мессенджера.
 * @param bot Ссылка на клиент `TgBot::Bot`.
 */
TgBotMessenger::TgBotMessenger(TgBot::Bot& bot)
    : bot_(bot) {
}

/**
 * @brief Отправка исходящего сообщения в указанный чат Telegram.
 * Перехватывает исключения API и стандартные ошибки во избежание падения приложения.
 * @param chatId Идентификатор целевого чата.
 * @param text Текст сообщения.
 */
void TgBotMessenger::sendMessage(int64_t chatId, const std::string& text) {
    try {
        bot_.getApi().sendMessage(chatId, text);
    } catch (const TgBot::TgException& e) {
        std::cerr << "[TgBotMessenger] Ошибка Telegram API при sendMessage: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[TgBotMessenger] Непредвиденная ошибка при sendMessage: " << e.what() << "\n";
    }
}

/**
 * @brief Подтверждение входящего callback-запроса от кнопки в Telegram.
 * @param callbackId Идентификатор подтверждаемого запроса.
 * @param text Опциональный текст уведомления.
 */
void TgBotMessenger::answerCallbackQuery(const std::string& callbackId, const std::string& text) {
    try {
        bot_.getApi().answerCallbackQuery(callbackId, text);
    } catch (const TgBot::TgException& e) {
        std::cerr << "[TgBotMessenger] Ошибка Telegram API при answerCallbackQuery: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[TgBotMessenger] Непредвиденная ошибка при answerCallbackQuery: " << e.what() << "\n";
    }
}

// --- Реализация TelegramBotApp ---

/**
 * @brief Конструктор хоста бота, создающий адаптер и подключающий маршрутизатор.
 * @param token Токен Telegram-бота.
 */
TelegramBotApp::TelegramBotApp(const std::string& token)
    : bot_(token),
      messenger_(std::make_shared<TgBotMessenger>(bot_)),
      router_(std::make_shared<BotEventRouter>(messenger_)),
      stateMachine_(std::make_shared<BotStateMachine>(messenger_)) {
    stateMachine_->attachToRouter(*router_);
    setupCallbacks();
}

/**
 * @brief Деструктор хоста приложения.
 */
TelegramBotApp::~TelegramBotApp() {
    stop();
}

/**
 * @brief Подписка на события библиотеки tgbot-cpp и конвертация их в структуры DTO.
 */
void TelegramBotApp::setupCallbacks() {
    bot_.getEvents().onAnyMessage([this](const TgBot::Message::Ptr& message) {
        if (!message) {
            return;
        }

        IncomingMessage msg;
        if (message->chat) {
            msg.chatId = message->chat->id;
        }
        if (message->from) {
            msg.userId = message->from->id;
            msg.username = message->from->username;
        }
        msg.text = message->text;

        router_->onTextMessage(msg);
    });

    bot_.getEvents().onCallbackQuery([this](const TgBot::CallbackQuery::Ptr& query) {
        if (!query) {
            return;
        }

        IncomingCallback cb;
        cb.callbackId = query->id;
        if (query->message && query->message->chat) {
            cb.chatId = query->message->chat->id;
        }
        if (query->from) {
            cb.userId = query->from->id;
        }
        if (query->message) {
            cb.messageId = query->message->messageId;
        }
        cb.data = query->data;

        router_->onCallbackQuery(cb);
    });
}

/**
 * @brief Запуск цикла опроса long-polling с обработкой сетевых сбоев.
 */
void TelegramBotApp::run() {
    isRunning_ = true;

    try {
        bot_.getApi().deleteWebhook();
    } catch (const TgBot::TgException& e) {
        std::cerr << "[TelegramBotApp] Уведомление: вызов deleteWebhook: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[TelegramBotApp] Ошибка при вызове deleteWebhook: " << e.what() << "\n";
    }

    try {
        std::cout << "[TelegramBotApp] Запуск long-polling для бота @"
                  << bot_.getApi().getMe()->username << "...\n";
    } catch (const std::exception& e) {
        std::cerr << "[TelegramBotApp] Предупреждение: не удалось получить имя бота: " << e.what() << "\n";
    }

    TgBot::TgLongPoll longPoll(bot_);
    while (isRunning_) {
        try {
            longPoll.start();
        } catch (const TgBot::TgException& e) {
            std::cerr << "[TelegramBotApp] Ошибка Telegram API в цикле опроса: " << e.what() << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[TelegramBotApp] Стандартное исключение в цикле опроса: " << e.what() << "\n";
        }
    }

    std::cout << "[TelegramBotApp] Цикл long-polling завершен.\n";
}

/**
 * @brief Остановка цикла long-polling.
 */
void TelegramBotApp::stop() {
    isRunning_ = false;
}

/**
 * @brief Возвращает указатель на маршрутизатор событий.
 * @return Умный указатель на `BotEventRouter`.
 */
std::shared_ptr<BotEventRouter> TelegramBotApp::getRouter() const {
    return router_;
}

/**
 * @brief Предоставляет доступ к конечному автомату диалогов.
 * @return Умный указатель на `BotStateMachine`.
 */
std::shared_ptr<BotStateMachine> TelegramBotApp::getStateMachine() const {
    return stateMachine_;
}

/**
 * @brief Предоставляет доступ к внутреннему объекту `TgBot::Bot`.
 * @return Ссылка на `TgBot::Bot`.
 */
TgBot::Bot& TelegramBotApp::getBot() {
    return bot_;
}

} // namespace bot
