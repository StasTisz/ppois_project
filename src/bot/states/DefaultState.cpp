#include "bot/states/DefaultState.h"
#include "bot/KeyboardFactory.h"

#include <sstream>

/**
 * @file DefaultState.cpp
 * @brief Реализация логики главного меню бота в состоянии BotState::Idle.
 */

namespace bot {

namespace {

std::string formatHistoryRatings(IDialogStateContext& ctx, const UserSession& session) {
    std::ostringstream oss;
    oss << "📜 Ваша история оценок:\n";
    for (const auto& [movieId, score] : session.getMovieRatings()) {
        auto movieOpt = ctx.getMovieById(movieId);
        std::string title = movieOpt ? movieOpt->title : ("Фильм #" + std::to_string(movieId));
        oss << "• " << title << " — " << score << "/10\n";
    }
    return oss.str();
}

} // namespace

BotState DefaultState::getStateType() const {
    return BotState::Idle;
}

void DefaultState::onEnter(IDialogStateContext& ctx, UserSession& session) {
    if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessageWithKeyboard(session.chatId,
            "Главное меню:\n"
            "• /recommend — Получить рекомендации фильмов\n"
            "• /genres — Настроить любимые жанры\n"
            "• /history — История оценок\n"
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
            "/genres — Изменить список любимых жанров\n"
            "/history — История просмотренных и оцененных фильмов\n"
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
    } else if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessageWithKeyboard(session.chatId,
            "Главное меню:\nИспользуйте кнопки меню ниже или команды /recommend и /genres.",
            KeyboardFactory::createMainMenuKeyboard());
    }
}

std::string DefaultState::formatHistoryText(IDialogStateContext& ctx, const UserSession& session) const {
    std::string strategy = ctx.getActiveStrategyName(session.userId);
    if (!session.hasRatings()) {
        std::string text = "Ваша история просмотров и оценок пока пуста.";
        return strategy.empty() ? text : (text + "\n🎯 Активный режим рекомендаций: " + strategy);
    }
    std::string text = formatHistoryRatings(ctx, session);
    if (!strategy.empty()) {
        text += "\n🎯 Активный режим рекомендаций: " + strategy + "\n";
    }
    return text + "\nИспользуйте меню ниже для дальнейших действий:";
}

void DefaultState::handleHistoryCallback(IDialogStateContext& ctx, const UserSession& session) {
    auto messenger = ctx.getMessenger();
    if (!messenger) return;
    std::string text = formatHistoryText(ctx, session);
    messenger->sendMessageWithKeyboard(session.chatId, text, KeyboardFactory::createMainMenuKeyboard());
}

void DefaultState::handleCallback(
    IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) {
    if (cb.data == "menu:genres") {
        ctx.transitionTo(session.userId, BotState::SelectingGenres);
    } else if (cb.data == "menu:recommend") {
        handleRecommendCommand(ctx, session);
    } else if (cb.data == "menu:history") {
        handleHistoryCallback(ctx, session);
    } else if (cb.data == "menu:help") {
        handleHelpCommand(ctx, session);
    }
}

} // namespace bot
