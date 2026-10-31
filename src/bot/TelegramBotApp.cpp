#include "bot/TelegramBotApp.h"
#include "bot/BotConstants.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

/**
 * @file TelegramBotApp.cpp
 * @brief Реализация цикла long-polling приложения TelegramBotApp и адаптера TgBotMessenger.
 */

namespace bot {

namespace {

TgBot::InlineKeyboardButton::Ptr createTgButton(const InlineButton& btn) {
    auto tgBtn = std::make_shared<TgBot::InlineKeyboardButton>();
    tgBtn->text = btn.text;
    if (!btn.url.empty()) tgBtn->url = btn.url;
    else tgBtn->callbackData = btn.callbackData;
    return tgBtn;
}

IncomingMessage extractMessageDto(const TgBot::Message::Ptr& message) {
    IncomingMessage msg;
    if (message->chat) msg.chatId = message->chat->id;
    if (message->from) {
        msg.userId = message->from->id;
        msg.username = message->from->username;
    }
    msg.text = message->text;
    return msg;
}

IncomingCallback extractCallbackDto(const TgBot::CallbackQuery::Ptr& query) {
    IncomingCallback cb;
    cb.callbackId = query->id;
    if (query->message && query->message->chat) cb.chatId = query->message->chat->id;
    if (query->from) cb.userId = query->from->id;
    if (query->message) cb.messageId = query->message->messageId;
    cb.data = query->data;
    return cb;
}

bool sendPhotoCall(
    TgBot::Bot& bot, int64_t chatId, const std::string& target, const std::string& caption,
    const TgBot::InlineKeyboardMarkup::Ptr& markup, const RetryConfig& mediaConfig,
    TgBot::Message::Ptr& sentMsg, std::string& lastError) {
    return NetworkErrorHandler::executeWithRetry(
        [&bot, chatId, &target, &caption, &markup, &sentMsg]() {
            sentMsg = bot.getApi().sendPhoto(chatId, target, caption, 0, markup);
        }, mediaConfig, &lastError);
}

std::vector<TgBot::BotCommand::Ptr> buildBotCommands() {
    std::vector<std::pair<std::string, std::string>> list = {
        {"start", "Запуск и выбор любимых жанров"},
        {"recommend", "Подбор рекомендаций фильмов"},
        {"genres", "Настройка списка любимых жанров"},
        {"history", "История оценок и активный алгоритм"},
        {"similar", "Поиск похожих фильмов во время просмотра"},
        {"stats", "Аналитика и метрики работы бота"},
        {"help", "Справка по возможностям бота"}
    };
    std::vector<TgBot::BotCommand::Ptr> commands;
    for (const auto& [cmd, desc] : list) {
        auto c = std::make_shared<TgBot::BotCommand>();
        c->command = cmd;
        c->description = desc;
        commands.push_back(c);
    }
    return commands;
}

} // namespace

// --- Реализация TgBotMessenger ---

TgBotMessenger::TgBotMessenger(TgBot::Bot& bot, RetryConfig retryConfig)
    : bot_(bot),
      retryConfig_(retryConfig) {
}

TgBot::InlineKeyboardMarkup::Ptr TgBotMessenger::buildMarkup(const InlineKeyboard& keyboard) const {
    if (keyboard.empty()) return nullptr;
    auto markup = std::make_shared<TgBot::InlineKeyboardMarkup>();
    markup->inlineKeyboard.reserve(keyboard.rows.size());
    for (const auto& row : keyboard.rows) {
        std::vector<TgBot::InlineKeyboardButton::Ptr> tgRow;
        tgRow.reserve(row.size());
        for (const auto& btn : row) tgRow.push_back(createTgButton(btn));
        markup->inlineKeyboard.push_back(std::move(tgRow));
    }
    return markup;
}

void TgBotMessenger::sendMessage(int64_t chatId, const std::string& text) {
    NetworkErrorHandler::executeWithRetry([this, chatId, &text]() {
        bot_.getApi().sendMessage(chatId, text);
    }, retryConfig_);
}

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

void TgBotMessenger::editMessageWithKeyboard(
    int64_t chatId, int32_t messageId, const std::string& text, const InlineKeyboard& keyboard) {
    if (messageId <= 0) {
        sendMessageWithKeyboard(chatId, text, keyboard);
        return;
    }
    auto markup = buildMarkup(keyboard);
    try {
        bot_.getApi().editMessageText(text, chatId, messageId, "", "", nullptr, markup);
    } catch (const TgBot::TgException& e) {
        std::string err = e.what();
        if (err.find("message is not modified") == std::string::npos) {
            std::cerr << "[TgBotMessenger] Предупреждение editMessageText: " << err << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "[TgBotMessenger] Ошибка editMessageText: " << e.what() << "\n";
    }
}

std::string TgBotMessenger::resolveCachedPhotoTarget(const std::string& photoUrlOrFileId) const {
    std::shared_lock<std::shared_mutex> readLock(cacheMutex_);
    auto it = photoFileIdCache_.find(photoUrlOrFileId);
    if (it != photoFileIdCache_.end() && !it->second.empty()) {
        return it->second;
    }
    return photoUrlOrFileId;
}

void TgBotMessenger::storeCachedFileId(
    const std::string& photoUrlOrFileId, const TgBot::Message::Ptr& sentMsg, std::string* fileIdOut) {
    if (!sentMsg || sentMsg->photo.empty() || !sentMsg->photo.back()) {
        return;
    }
    std::string obtainedId = sentMsg->photo.back()->fileId;
    {
        std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);
        photoFileIdCache_[photoUrlOrFileId] = obtainedId;
    }
    if (fileIdOut) {
        *fileIdOut = obtainedId;
    }
}

bool TgBotMessenger::sendPhotoWithKeyboard(
    int64_t chatId, const std::string& photoUrlOrFileId, const std::string& caption,
    const InlineKeyboard& keyboard, std::string* fileIdOut) {
    auto markup = buildMarkup(keyboard);
    std::string lastError;
    TgBot::Message::Ptr sentMsg;
    RetryConfig mediaConfig{kMediaMaxRetries, kMediaRetryDelayMs};

    std::string photoTarget = resolveCachedPhotoTarget(photoUrlOrFileId);
    bool ok = sendPhotoCall(bot_, chatId, photoTarget, caption, markup, mediaConfig, sentMsg, lastError);
    if (!ok && photoTarget != photoUrlOrFileId) {
        {
            std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);
            photoFileIdCache_.erase(photoUrlOrFileId);
        }
        ok = sendPhotoCall(bot_, chatId, photoUrlOrFileId, caption, markup, mediaConfig, sentMsg, lastError);
    }
    if (ok) storeCachedFileId(photoUrlOrFileId, sentMsg, fileIdOut);
    return ok;
}

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

void TgBotMessenger::cachePhotoFileId(const std::string& photoUrl, const std::string& fileId) {
    if (!photoUrl.empty() && !fileId.empty()) {
        std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);
        photoFileIdCache_[photoUrl] = fileId;
    }
}

std::string TgBotMessenger::getCachedPhotoFileId(const std::string& photoUrl) const {
    std::shared_lock<std::shared_mutex> readLock(cacheMutex_);
    auto it = photoFileIdCache_.find(photoUrl);
    return (it != photoFileIdCache_.end()) ? it->second : "";
}

// --- Реализация TelegramBotApp ---

TelegramBotApp::TelegramBotApp(const BotConfig& config)
    : config_(config),
      bot_(config.token),
      messenger_(std::make_shared<TgBotMessenger>(bot_)),
      router_(std::make_shared<BotEventRouter>(messenger_)),
      stateMachine_(std::make_shared<BotStateMachine>(messenger_)),
      threadPool_(config.threadPoolSize > 0 ? config.threadPoolSize : kDefaultThreadPoolSize) {
    stateMachine_->setStorageChatId(config.storageChatId);
    stateMachine_->attachToRouter(*router_);
    setupCallbacks();
}

TelegramBotApp::TelegramBotApp(
    const BotConfig& config,
    std::shared_ptr<recommendation::RecommendationService> recService,
    std::shared_ptr<repository::IUserRepository> userRepo)
    : config_(config),
      bot_(config.token),
      messenger_(std::make_shared<TgBotMessenger>(bot_)),
      router_(std::make_shared<BotEventRouter>(messenger_)),
      stateMachine_(std::make_shared<BotStateMachine>(messenger_, std::move(recService), std::move(userRepo))),
      threadPool_(config.threadPoolSize > 0 ? config.threadPoolSize : kDefaultThreadPoolSize) {
    stateMachine_->setStorageChatId(config.storageChatId);
    stateMachine_->attachToRouter(*router_);
    setupCallbacks();
}

TelegramBotApp::TelegramBotApp(const std::string& token)
    : TelegramBotApp(BotConfig{token}) {
}

TelegramBotApp::~TelegramBotApp() {
    stop();
}

void TelegramBotApp::handleIncomingMessage(const TgBot::Message::Ptr& message) {
    if (!message || (message->from && message->from->isBot)) return;
    if (config_.storageChatId != 0 && message->chat && message->chat->id == config_.storageChatId) {
        std::cout << "[TelegramBotApp] Пропущено сервисное сообщение из чата-хранилища ("
                  << config_.storageChatId << ").\n";
        return;
    }

    IncomingMessage msg = extractMessageDto(message);
    std::cout << "[TelegramBotApp] 📩 Входящее сообщение от @"
              << (msg.username.empty() ? std::to_string(msg.userId) : msg.username)
              << " (chat=" << msg.chatId << "): \"" << msg.text << "\"\n";

    threadPool_.enqueue([this, msg]() {
        try { router_->onTextMessage(msg); }
        catch (const std::exception& e) {
            std::cerr << "[TelegramBotApp] Исключение в пуле потоков: " << e.what() << "\n";
        }
    });
}

void TelegramBotApp::handleIncomingCallback(const TgBot::CallbackQuery::Ptr& query) {
    if (!query) return;
    if (config_.storageChatId != 0 && query->message && query->message->chat &&
        query->message->chat->id == config_.storageChatId) {
        return;
    }

    IncomingCallback cb = extractCallbackDto(query);
    std::cout << "[TelegramBotApp] 🔘 Нажата inline-кнопка от user=" << cb.userId
              << " (chat=" << cb.chatId << "): data=\"" << cb.data << "\"\n";

    threadPool_.enqueue([this, cb]() {
        try { router_->onCallbackQuery(cb); }
        catch (const std::exception& e) {
            std::cerr << "[TelegramBotApp] Исключение в пуле потоков: " << e.what() << "\n";
        }
    });
}

void TelegramBotApp::setupCallbacks() {
    bot_.getEvents().onAnyMessage([this](const TgBot::Message::Ptr& msg) { handleIncomingMessage(msg); });
    bot_.getEvents().onCallbackQuery([this](const TgBot::CallbackQuery::Ptr& q) { handleIncomingCallback(q); });
}

void TelegramBotApp::registerBotCommands() {
    try {
        bot_.getApi().setMyCommands(buildBotCommands());
        std::cout << "[TelegramBotApp] Меню команд бота успешно зарегистрировано в Telegram API.\n";
    } catch (const std::exception& e) {
        std::cerr << "[TelegramBotApp] Предупреждение: не удалось зарегистрировать команды: " << e.what() << "\n";
    }
}

void TelegramBotApp::logStartupInfo() {
    try {
        bot_.getApi().deleteWebhook();
    } catch (...) {}
    try {
        auto me = bot_.getApi().getMe();
        std::cout << "[TelegramBotApp] Запуск long-polling для @" << (me ? me->username : "bot") << "...\n";
    } catch (...) {}
    registerBotCommands();
    std::cout << "[TelegramBotApp] ✅ Бот готов к работе и слушает входящие события в Telegram!\n";
}

void TelegramBotApp::run() {
    isRunning_ = true;
    logStartupInfo();
    TgBot::TgLongPoll longPoll(bot_, 100, config_.longPollTimeoutSeconds);
    while (isRunning_) {
        try {
            longPoll.start();
        } catch (const TgBot::TgException& e) {
            auto errType = NetworkErrorHandler::classifyError(e.what());
            std::cerr << "[TelegramBotApp] Ошибка Telegram API (" << static_cast<int>(errType)
                      << "): " << e.what() << "\n";
        } catch (const std::exception& e) {
            auto errType = NetworkErrorHandler::classifyError(e.what());
            std::cerr << "[TelegramBotApp] Стандартное исключение (" << static_cast<int>(errType)
                      << "): " << e.what() << "\n";
        }
    }
    std::cout << "[TelegramBotApp] Цикл long-polling завершен.\n";
}

void TelegramBotApp::stop() {
    isRunning_ = false;
    threadPool_.shutdown();
}

std::shared_ptr<BotEventRouter> TelegramBotApp::getRouter() const { return router_; }
std::shared_ptr<BotStateMachine> TelegramBotApp::getStateMachine() const { return stateMachine_; }
std::shared_ptr<TgBotMessenger> TelegramBotApp::getMessenger() const { return messenger_; }
TgBot::Bot& TelegramBotApp::getBot() { return bot_; }
void TelegramBotApp::startMediaCacheWarmup() {}

} // namespace bot
