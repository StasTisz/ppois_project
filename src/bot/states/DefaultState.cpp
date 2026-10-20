#include "bot/states/DefaultState.h"
#include "bot/KeyboardFactory.h"

/**
 * @file DefaultState.cpp
 * @brief Реализация логики главного меню бота в состоянии BotState::Idle.
 */

namespace bot {

BotState DefaultState::getStateType() const {
    return BotState::Idle;
}

void DefaultState::onEnter(IDialogStateContext& ctx, UserSession& session) {
    auto messenger = ctx.getMessenger();
    if (!messenger) {
        return;
    }

    messenger->sendMessageWithKeyboard(session.chatId,
        "Главное меню:\n"
        "• /recommend — Получить рекомендации фильмов\n"
        "• /genres — Настроить любимые жанры\n"
        "• /history — История оценок\n"
        "• /help — Справка по командам",
        KeyboardFactory::createMainMenuKeyboard());
}

void DefaultState::handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) {
    auto messenger = ctx.getMessenger();

    if (msg.text == "/start" || msg.text == "/genres") {
        ctx.transitionTo(session.userId, BotState::SelectingGenres);
        return;
    }

    if (msg.text == "/recommend") {
        if (session.selectedGenres.empty()) {
            if (messenger) {
                messenger->sendMessage(session.chatId,
                    "Для подбора фильмов сначала выберите хотя бы один любимый жанр.");
            }
            ctx.transitionTo(session.userId, BotState::SelectingGenres);
        } else {
            ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
        }
        return;
    }

    if (msg.text == "/help") {
        if (messenger) {
            messenger->sendMessageWithKeyboard(session.chatId,
                "Справка бота рекомендаций фильмов:\n"
                "/recommend — Начать подбор рекомендаций на основе ваших предпочтений\n"
                "/genres — Изменить список любимых жанров\n"
                "/history — История просмотренных и оцененных фильмов\n"
                "/help — Справка по командам\n"
                "/menu — Вернуться в главное меню",
                KeyboardFactory::createMainMenuKeyboard());
        }
        return;
    }

    if (messenger) {
        messenger->sendMessageWithKeyboard(session.chatId,
            "Главное меню:\n"
            "Используйте кнопки меню ниже или команды /recommend и /genres.",
            KeyboardFactory::createMainMenuKeyboard());
    }
}

void DefaultState::handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) {
    if (cb.data == "menu:genres") {
        ctx.transitionTo(session.userId, BotState::SelectingGenres);
        return;
    }

    if (cb.data == "menu:recommend") {
        if (session.selectedGenres.empty()) {
            if (auto messenger = ctx.getMessenger()) {
                messenger->sendMessage(session.chatId,
                    "Для подбора фильмов сначала выберите хотя бы один любимый жанр.");
            }
            ctx.transitionTo(session.userId, BotState::SelectingGenres);
        } else {
            ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
        }
        return;
    }
}

} // namespace bot
