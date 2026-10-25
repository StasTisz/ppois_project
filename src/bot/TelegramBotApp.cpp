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
    const std::string& photoUrl,
    const std::string& caption,
    const InlineKeyboard& keyboard) {
    auto markup = buildMarkup(keyboard);
    std::string lastError;

    // Для внешних ссылок на фото не делаем повторных блокирующих попыток,
    // чтобы при сетевом сбое TMDB бот не зависал, а сразу отдавал текстовую карточку
    RetryConfig mediaConfig = retryConfig_;
    mediaConfig.maxRetries = 0;

    bool success = NetworkErrorHandler::executeWithRetry([this, chatId, &photoUrl, &caption, &markup]() {
        bot_.getApi().sendPhoto(chatId, photoUrl, caption, 0, markup);
    }, mediaConfig, &lastError);

    if (!success) {
        std::cerr << "[TgBotMessenger] Ошибка отправки постера (" << photoUrl
                  << "): " << lastError << ". Выполняется fallback на текстовое сообщение.\n";
        sendMessageWithKeyboard(chatId, caption, keyboard);
        return false;
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
      stateMachine_(std::make_shared<BotStateMachine>(messenger_)) {
    stateMachine_->attachToRouter(*router_);
    setupCallbacks();
}

/**
 * @brief Конструктор приложения с сервисом рекомендаций и репозиторием пользователей.
 * @param config Настройки приложения и токен Telegram-бота.
 * @param recService Сервис-фасад рекомендаций фильмов.
 * @param userRepo Репозиторий пользователей для синхронизации в SQLite.
 */
TelegramBotApp::TelegramBotApp(
    const BotConfig& config,
    std::shared_ptr<recommendation::RecommendationService> recService,
    std::shared_ptr<repository::IUserRepository> userRepo)
    : config_(config),
      bot_(config.token),
      messenger_(std::make_shared<TgBotMessenger>(bot_)),
      router_(std::make_shared<BotEventRouter>(messenger_)),
      stateMachine_(std::make_shared<BotStateMachine>(messenger_, std::move(recService), std::move(userRepo))) {
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

    try {
        std::vector<TgBot::BotCommand::Ptr> commands;

        auto addCmd = [&commands](const std::string& cmd, const std::string& desc) {
            auto c = std::make_shared<TgBot::BotCommand>();
            c->command = cmd;
            c->description = desc;
            commands.push_back(c);
        };

        addCmd("start", "Запуск и выбор любимых жанров");
        addCmd("recommend", "Подбор рекомендаций фильмов");
        addCmd("genres", "Настройка списка любимых жанров");
        addCmd("history", "История оценок и активный алгоритм");
        addCmd("similar", "Поиск похожих фильмов во время просмотра");
        addCmd("help", "Справка по возможностям бота");

        bot_.getApi().setMyCommands(commands);
        std::cout << "[TelegramBotApp] Меню команд бота успешно зарегистрировано в Telegram API.\n";
    } catch (const std::exception& e) {
        std::cerr << "[TelegramBotApp] Предупреждение: не удалось зарегистрировать команды: " << e.what() << "\n";
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
