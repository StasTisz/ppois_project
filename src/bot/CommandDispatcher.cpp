#include "bot/CommandDispatcher.h"
#include "bot/KeyboardFactory.h"

#include <sstream>
#include <utility>

/**
 * @file CommandDispatcher.cpp
 * @brief Реализация диспетчера команд CommandDispatcher.
 */

namespace bot {

CommandDispatcher::CommandDispatcher(IDialogStateContext& context, std::shared_ptr<IBotMessenger> messenger)
    : context_(context),
      messenger_(std::move(messenger)) {
}

std::string CommandDispatcher::extractCommand(const std::string& text) {
    if (text.empty() || text[0] != '/') {
        return "";
    }

    auto spacePos = text.find(' ');
    std::string cmd = (spacePos == std::string::npos) ? text : text.substr(0, spacePos);

    auto atPos = cmd.find('@');
    if (atPos != std::string::npos) {
        cmd = cmd.substr(0, atPos);
    }

    return cmd;
}

bool CommandDispatcher::dispatch(const IncomingMessage& msg) {
    std::string cmd = extractCommand(msg.text);
    if (cmd.empty()) {
        return false;
    }

    auto& session = context_.getSession(msg.userId, msg.chatId, msg.username);

    if (cmd == "/start") {
        handleStart(session);
        return true;
    }
    if (cmd == "/recommend") {
        handleRecommend(session);
        return true;
    }
    if (cmd == "/genres") {
        handleGenres(session);
        return true;
    }
    if (cmd == "/history") {
        handleHistory(session);
        return true;
    }
    if (cmd == "/stats") {
        handleStats(session);
        return true;
    }
    if (cmd == "/help") {
        handleHelp(session);
        return true;
    }

    if (cmd == "/menu") {
        context_.transitionTo(session.userId, BotState::Idle);
        return true;
    }

    return false;
}

void CommandDispatcher::handleStart(UserSession& session) {
    context_.transitionTo(session.userId, BotState::SelectingGenres);
}

void CommandDispatcher::handleRecommend(UserSession& session) {
    if (session.selectedGenres.empty()) {
        if (messenger_) {
            messenger_->sendMessage(session.chatId,
                "⚠️ Для подбора рекомендаций сначала выберите хотя бы один любимый жанр.");
        }
        context_.transitionTo(session.userId, BotState::SelectingGenres);
    } else {
        context_.transitionTo(session.userId, BotState::BrowsingRecommendations);
    }
}

void CommandDispatcher::handleGenres(UserSession& session) {
    context_.transitionTo(session.userId, BotState::SelectingGenres);
}

void CommandDispatcher::handleHistory(const UserSession& session) {
    if (!messenger_) {
        return;
    }

    std::string strategy = context_.getActiveStrategyName(session.userId);

    if (session.movieRatings.empty()) {
        std::string text = "Ваша история просмотров и оценок пока пуста.";
        if (!strategy.empty()) {
            text += "\n🎯 Активный режим рекомендаций: " + strategy;
        }
        messenger_->sendMessageWithKeyboard(
            session.chatId,
            text,
            KeyboardFactory::createMainMenuKeyboard());
        return;
    }

    std::ostringstream oss;
    oss << "📜 Ваша история оценок:\n";
    for (const auto& [movieId, score] : session.movieRatings) {
        auto movieOpt = context_.getMovieById(movieId);
        std::string title = movieOpt ? movieOpt->title : ("Фильм #" + std::to_string(movieId));
        oss << "• " << title << " — " << score << "/10\n";
    }

    if (!strategy.empty()) {
        oss << "\n🎯 Активный режим рекомендаций: " << strategy << "\n";
    }

    oss << "\nИспользуйте меню ниже для дальнейших действий:";
    messenger_->sendMessageWithKeyboard(
        session.chatId,
        oss.str(),
        KeyboardFactory::createMainMenuKeyboard());
}

void CommandDispatcher::handleStats(const UserSession& session) {
    if (!messenger_) {
        return;
    }

    std::string report = context_.getMetricsReport();
    if (report.empty()) {
        report = "📊 Метрики пока не собраны (коллектор не инициализирован).";
    }

    report += "\n\nИспользуйте меню ниже для дальнейших действий:";
    messenger_->sendMessageWithKeyboard(
        session.chatId,
        report,
        KeyboardFactory::createMainMenuKeyboard());
}

void CommandDispatcher::handleHelp(const UserSession& session) {
    if (!messenger_) {
        return;
    }

    std::string helpText =
        "ℹ️ Справка по командам и возможностям бота:\n\n"
        "📋 Команды:\n"
        "• /start — Запуск онбординга и выбор любимых жанров\n"
        "• /recommend — Подбор персональных рекомендаций фильмов\n"
        "• /genres — Настройка и смена списка любимых жанров\n"
        "• /history — Просмотр истории оцененных фильмов и активного алгоритма\n"
        "• /similar — Поиск похожих фильмов во время просмотра\n"
        "• /stats — Продуктовые метрики рекомендаций (CTR, DAU, время ответа)\n"
        "• /help — Справка по всем возможностям бота\n\n"
        "🎬 Кнопки под карточкой фильма:\n"
        "• 👍 Понравился — выставляет 10/10 и переключает на Content-Based подбор\n"
        "• 👎 Не понравился — выставляет 1/10 и исключает фильм из рекомендаций\n"
        "• ⭐ Оценить — выбор точной оценки от 1 до 10\n"
        "• ⏭ Следующий — пропуск фильма без выставления оценки\n"
        "• 🔍 Похожие — быстрый поиск похожих картин по режиссеру и жанрам\n\n"
        "🧠 Стратегии рекомендаций:\n"
        "1. Cold-Start / Popularity — топовые фильмы каталога в выбранных вами жанрах.\n"
        "2. Content-Based Filtering — умный подбор по сходству жанров (70%), режиссера (25%) и эпохи (5%).\n\n"
        "Используйте кнопки меню ниже для быстрой навигации:";
    messenger_->sendMessageWithKeyboard(
        session.chatId,
        helpText,
        KeyboardFactory::createMainMenuKeyboard());
}

} // namespace bot
