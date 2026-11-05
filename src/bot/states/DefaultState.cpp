#include "bot/states/DefaultState.h"
#include "bot/CommandDispatcher.h"
#include "bot/KeyboardFactory.h"
#include "bot/MovieCardFormatter.h"

#include <sstream>

/**
 * @file DefaultState.cpp
 * @brief Реализация логики главного меню бота в состоянии BotState::Idle.
 */

namespace bot {

BotState DefaultState::getStateType() const {
    return BotState::Idle;
}

void DefaultState::onEnter(IDialogStateContext& ctx, UserSession& session) {
    if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessageWithKeyboard(session.chatId,
            "Главное меню:\n"
            "• /recommend — Получить рекомендации фильмов\n"
            "• /search <название> — Поиск фильмов по названию\n"
            "• /genres — Настроить любимые жанры\n"
            "• /history — История оценок\n"
            "• /bookmarks — Список «Буду смотреть»\n"
            "• /help — Справка по командам",
            KeyboardFactory::createMainMenuKeyboard());
    }
}

void DefaultState::handleRecommendCommand(IDialogStateContext& ctx, UserSession& session) {
    if (!session.hasSelectedGenres()) {
        if (auto messenger = ctx.getMessenger()) {
            messenger->sendMessage(session.chatId,
                "Для подбора фильмов сначала выберите хотя бы один любимый жанр.");
        }
        ctx.transitionTo(session.userId, BotState::SelectingGenres);
        return;
    }
    ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
}

void DefaultState::handleHelpCommand(IDialogStateContext& ctx, const UserSession& session) {
    if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessageWithKeyboard(session.chatId,
            "Справка бота рекомендаций фильмов:\n"
            "/recommend — Начать подбор рекомендаций на основе ваших предпочтений\n"
            "/search <название> — Найти фильм по названию\n"
            "/genres — Изменить список любимых жанров\n"
            "/history — История просмотренных и оцененных фильмов\n"
            "/bookmarks — Список фильмов «Буду смотреть»\n"
            "/similar — Поиск похожих фильмов во время просмотра\n"
            "/help — Справка по командам\n"
            "/menu — Вернуться в главное меню",
            KeyboardFactory::createMainMenuKeyboard());
    }
}

void DefaultState::handleMessage(
    IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) {
    if (msg.text == "/start" || msg.text == "/genres") {
        ctx.transitionTo(session.userId, BotState::SelectingGenres);
    } else if (msg.text == "/recommend") {
        handleRecommendCommand(ctx, session);
    } else if (msg.text == "/menu") {
        onEnter(ctx, session);
    } else if (msg.text == "/help") {
        handleHelpCommand(ctx, session);
    } else if (msg.text == "/bookmarks" || msg.text == "/watchlist") {
        handleBookmarksCallback(ctx, session);
    } else if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessageWithKeyboard(session.chatId,
            "Главное меню:\nИспользуйте кнопки меню ниже или команды /recommend и /genres.",
            KeyboardFactory::createMainMenuKeyboard());
    }
}

std::string DefaultState::formatHistoryText(IDialogStateContext& ctx, const UserSession& session) const {
    std::string strategy = ctx.getActiveStrategyName(session.userId);
    return CommandDispatcher::formatHistoryText(session, strategy, ctx);
}

void DefaultState::handleHistoryCallback(IDialogStateContext& ctx, const UserSession& session) {
    auto messenger = ctx.getMessenger();
    if (!messenger) return;
    std::string strategy = ctx.getActiveStrategyName(session.userId);
    std::string text = CommandDispatcher::formatHistoryText(session, strategy, ctx);
    if (!session.hasRatings()) {
        messenger->sendMessageWithKeyboard(session.chatId, text, KeyboardFactory::createMainMenuKeyboard());
        return;
    }
    auto buttonItems = CommandDispatcher::buildHistoryButtonItems(session, ctx);
    messenger->sendMessageWithKeyboard(
        session.chatId, text, KeyboardFactory::createHistoryKeyboard(buttonItems));
}

void DefaultState::handleBookmarksCallback(IDialogStateContext& ctx, const UserSession& session) {
    auto messenger = ctx.getMessenger();
    if (!messenger) return;
    std::string text = CommandDispatcher::formatBookmarksText(session, ctx);
    messenger->sendMessageWithKeyboard(session.chatId, text, KeyboardFactory::createMainMenuKeyboard());
}

void DefaultState::handleSearchViewCallback(
    IDialogStateContext& ctx, UserSession& session, const std::string& data) {
    auto messenger = ctx.getMessenger();
    if (!messenger) return;
    int64_t movieId = 0;
    try {
        movieId = std::stoll(data.substr(std::string("search:view:").size()));
    } catch (...) { return; }
    auto movieOpt = ctx.getMovieById(movieId);
    if (!movieOpt) return;
    session.setCurrentMovie(*movieOpt);
    session.state = BotState::BrowsingRecommendations;
    bool isBookmarked = session.isBookmarked(movieOpt->id);
    auto kb = KeyboardFactory::createMovieReactionKeyboard(movieOpt->id, movieOpt->trailerUrl, isBookmarked);
    MovieCardFormatter::sendMovieCard(*messenger, session.chatId, *movieOpt, kb, "🔍 Найденный фильм:");
}

void DefaultState::handleCallback(
    IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) {
    if (cb.data == "menu:genres") {
        ctx.transitionTo(session.userId, BotState::SelectingGenres);
    } else if (cb.data == "menu:recommend") {
        handleRecommendCommand(ctx, session);
    } else if (cb.data == "menu:history") {
        handleHistoryCallback(ctx, session);
    } else if (cb.data == "menu:bookmarks") {
        handleBookmarksCallback(ctx, session);
    } else if (cb.data == "menu:help") {
        handleHelpCommand(ctx, session);
    } else if (cb.data == "menu:main") {
        onEnter(ctx, session);
    } else if (cb.data.rfind("search:view:", 0) == 0) {
        handleSearchViewCallback(ctx, session, cb.data);
    }
}

} // namespace bot
