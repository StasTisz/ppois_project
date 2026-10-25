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
    if (cmd == "/help") {
        handleHelp(session);
        return true;
    }

    return false;
}

void CommandDispatcher::handleStart(UserSession& session) {
    if (messenger_) {
        std::string welcome = "👋 Добро пожаловать" +
            (session.username.empty() ? "" : (", @" + session.username)) +
            " в Movie Recommendation Bot!\n"
            "Я помогу найти отличные фильмы по вашим предпочтениям.";
        messenger_->sendMessage(session.chatId, welcome);
    }
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

    if (session.movieRatings.empty()) {
        messenger_->sendMessageWithKeyboard(
            session.chatId,
            "Ваша история просмотров и оценок пока пуста.",
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
    oss << "\nИспользуйте меню ниже для дальнейших действий:";
    messenger_->sendMessageWithKeyboard(
        session.chatId,
        oss.str(),
        KeyboardFactory::createMainMenuKeyboard());
}

void CommandDispatcher::handleHelp(const UserSession& session) {
    if (!messenger_) {
        return;
    }

    std::string helpText =
        "ℹ️ Справка по командам бота:\n"
        "• /start — Запуск онбординга и выбор любимых жанров\n"
        "• /recommend — Подбор персональных рекомендаций фильмов\n"
        "• /genres — Настройка и смена списка любимых жанров\n"
        "• /history — Просмотр истории оцененных фильмов\n"
        "• /help — Справка по всем возможностям бота\n\n"
        "Используйте кнопки меню ниже для быстрой навигации:";
    messenger_->sendMessageWithKeyboard(
        session.chatId,
        helpText,
        KeyboardFactory::createMainMenuKeyboard());
}

} // namespace bot
