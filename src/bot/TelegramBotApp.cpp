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
 * @param retryConfig Параметры политики повторных попыток при сетевых сбоях.
 */
TgBotMessenger::TgBotMessenger(TgBot::Bot& bot, RetryConfig retryConfig)
    : bot_(bot),
      retryConfig_(retryConfig) {
}

/**
 * @brief Вспомогательный метод построения разметки inline-клавиатуры tgbot-cpp.
 * @param keyboard Входная структура кнопок InlineKeyboard.
 * @return Умный указатель на TgBot::InlineKeyboardMarkup.
 */
TgBot::InlineKeyboardMarkup::Ptr TgBotMessenger::buildMarkup(const InlineKeyboard& keyboard) const {
    if (keyboard.empty()) {
        return nullptr;
    }

    auto markup = std::make_shared<TgBot::InlineKeyboardMarkup>();
    markup->inlineKeyboard.reserve(keyboard.rows.size());

    for (const auto& row : keyboard.rows) {
        std::vector<TgBot::InlineKeyboardButton::Ptr> tgRow;
        tgRow.reserve(row.size());
        for (const auto& btn : row) {
            auto tgBtn = std::make_shared<TgBot::InlineKeyboardButton>();
            tgBtn->text = btn.text;
            tgBtn->callbackData = btn.callbackData;
            tgRow.push_back(std::move(tgBtn));
        }
        markup->inlineKeyboard.push_back(std::move(tgRow));
    }

    return markup;
}

/**
 * @brief Отправка исходящего сообщения в указанный чат Telegram с политикой повторов.
 * @param chatId Идентификатор целевого чата.
 * @param text Текст сообщения.
 */
void TgBotMessenger::sendMessage(int64_t chatId, const std::string& text) {
    NetworkErrorHandler::executeWithRetry([this, chatId, &text]() {
        bot_.getApi().sendMessage(chatId, text);
    }, retryConfig_);
}

/**
 * @brief Отправка сообщения с inline-клавиатурой в чат Telegram с политикой повторов.
 * @param chatId Идентификатор целевого чата.
 * @param text Текст сообщения.
 * @param keyboard Структура inline-клавиатуры.
 */
void TgBotMessenger::sendMessageWithKeyboard(int64_t chatId, const std::string& text, const InlineKeyboard& keyboard) {
    if (keyboard.empty()) {
        sendMessage(chatId, text);
        return;
    }

    auto markup = buildMarkup(keyboard);
    NetworkErrorHandler::executeWithRetry([this, chatId, &text, &markup]() {
        bot_.getApi().sendMessage(chatId, text, nullptr, nullptr, markup);
    }, retryConfig_);
}

/**
 * @brief Отправляет фото (постер) с подписью и inline-клавиатурой в чат Telegram.
 * При сетевой ошибке или недоступности постера автоматически выполняет fallback на sendMessageWithKeyboard.
 * @param chatId Идентификатор целевого чата.
 * @param photoUrl URL постера фильма.
 * @param caption Описание/подпись к фильму.
 * @param keyboard Структура кнопок реакций.
 * @return true при успешной отправке фото, false если сработал fallback на текст.
 */
bool TgBotMessenger::sendPhotoWithKeyboard(
    int64_t chatId,
    const std::string& photoUrlOrFileId,
    const std::string& caption,
    const InlineKeyboard& keyboard,
    std::string* fileIdOut) {
    auto markup = buildMarkup(keyboard);
    std::string lastError;
    TgBot::Message::Ptr sentMsg;

    // Для внешних ссылок на фото не делаем повторных блокирующих попыток,
    // чтобы при сетевом сбое TMDB бот не зависал, а сразу отдавал текстовую карточку
    RetryConfig mediaConfig = retryConfig_;
    mediaConfig.maxRetries = 0;

    bool success = NetworkErrorHandler::executeWithRetry([this, chatId, &photoUrlOrFileId, &caption, &markup, &sentMsg]() {
        sentMsg = bot_.getApi().sendPhoto(chatId, photoUrlOrFileId, caption, nullptr, markup);
    }, mediaConfig, &lastError);

    if (!success) {
        std::cerr << "[TgBotMessenger] Ошибка отправки постера (" << photoUrlOrFileId
                  << "): " << lastError << ".\n";
        return false;
    }

    if (fileIdOut && sentMsg && !sentMsg->photo.empty()) {
        *fileIdOut = sentMsg->photo.back()->fileId;
    }

    return true;
}

/**
 * @brief Подтверждение входящего callback-запроса от кнопки в Telegram.
 * @param callbackId Идентификатор подтверждаемого запроса.
 * @param text Опциональный текст уведомления.
 */
void TgBotMessenger::answerCallbackQuery(const std::string& callbackId, const std::string& text) {
    NetworkErrorHandler::executeWithRetry([this, &callbackId, &text]() {
        bot_.getApi().answerCallbackQuery(callbackId, text);
    }, retryConfig_);
}

const RetryConfig& TgBotMessenger::getRetryConfig() const {
    return retryConfig_;
}

void TgBotMessenger::setRetryConfig(const RetryConfig& config) {
    retryConfig_ = config;
}

// --- Реализация TelegramBotApp ---

/**
 * @brief Конструктор хоста бота с полной конфигурацией.
 * @param config Настройки приложения и токен Telegram-бота.
 */
TelegramBotApp::TelegramBotApp(const BotConfig& config)
    : config_(config),
      bot_(config.token),
      messenger_(std::make_shared<TgBotMessenger>(bot_)),
      router_(std::make_shared<BotEventRouter>(messenger_)),
      stateMachine_(std::make_shared<BotStateMachine>(messenger_)),
      threadPool_(config.threadPoolSize > 0 ? config.threadPoolSize : 4) {
    stateMachine_->setStorageChatId(config.storageChatId);
    stateMachine_->attachToRouter(*router_);
    setupCallbacks();
}

/**
 * @brief Конструктор хоста бота по токену (обратная совместимость).
 * @param token Токен Telegram-бота.
 */
TelegramBotApp::TelegramBotApp(const std::string& token)
    : TelegramBotApp(BotConfig{token}) {
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

        // Игнорируем сообщения из чата-хранилища постеров во избежание зацикливания
        if (config_.storageChatId != 0 && message->chat && message->chat->id == config_.storageChatId) {
            return;
        }

        // Игнорируем сообщения от ботов (включая сообщения от самого себя)
        if (message->from && message->from->isBot) {
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

        threadPool_.enqueue([this, msg]() {
            try {
                router_->onTextMessage(msg);
            } catch (const std::exception& e) {
                std::cerr << "[TelegramBotApp] Исключение в пуле потоков (onTextMessage): " << e.what() << "\n";
            }
        });
    });

    bot_.getEvents().onCallbackQuery([this](const TgBot::CallbackQuery::Ptr& query) {
        if (!query) {
            return;
        }

        // Игнорируем callback-события из чата-хранилища
        if (config_.storageChatId != 0 && query->message && query->message->chat && query->message->chat->id == config_.storageChatId) {
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

        threadPool_.enqueue([this, cb]() {
            try {
                router_->onCallbackQuery(cb);
            } catch (const std::exception& e) {
                std::cerr << "[TelegramBotApp] Исключение в пуле потоков (onCallbackQuery): " << e.what() << "\n";
            }
        });
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

    TgBot::TgLongPoll longPoll(bot_, 100, config_.longPollTimeoutSeconds);
    while (isRunning_) {
        try {
            longPoll.start();
        } catch (const TgBot::TgException& e) {
            auto errType = NetworkErrorHandler::classifyError(e.what());
            std::cerr << "[TelegramBotApp] Ошибка Telegram API в цикле опроса ("
                      << static_cast<int>(errType) << "): " << e.what() << "\n";
        } catch (const std::exception& e) {
            auto errType = NetworkErrorHandler::classifyError(e.what());
            std::cerr << "[TelegramBotApp] Стандартное исключение в цикле опроса ("
                      << static_cast<int>(errType) << "): " << e.what() << "\n";
        }
    }

    std::cout << "[TelegramBotApp] Цикл long-polling завершен.\n";
}

/**
 * @brief Остановка цикла long-polling.
 */
void TelegramBotApp::stop() {
    isRunning_ = false;
    threadPool_.shutdown();
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
 * @brief Предоставляет доступ к клиенту отправки сообщений Telegram.
 * @return Умный указатель на `TgBotMessenger`.
 */
std::shared_ptr<TgBotMessenger> TelegramBotApp::getMessenger() const {
    return messenger_;
}

/**
 * @brief Предоставляет доступ к внутреннему объекту `TgBot::Bot`.
 * @return Ссылка на `TgBot::Bot`.
 */
TgBot::Bot& TelegramBotApp::getBot() {
    return bot_;
}

} // namespace bot
