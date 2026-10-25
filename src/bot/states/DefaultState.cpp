#include "bot/states/DefaultState.h"
#include "bot/KeyboardFactory.h"

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
                "/similar — Поиск похожих фильмов во время просмотра\n"
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

    if (cb.data == "menu:history") {
        auto messenger = ctx.getMessenger();
        if (!messenger) {
            return;
        }

        std::string strategy = ctx.getActiveStrategyName(session.userId);

        if (session.movieRatings.empty()) {
            std::string text = "Ваша история просмотров и оценок пока пуста.";
            if (!strategy.empty()) {
                text += "\n🎯 Активный режим рекомендаций: " + strategy;
            }
            messenger->sendMessageWithKeyboard(
                session.chatId,
                text,
                KeyboardFactory::createMainMenuKeyboard());
        } else {
            std::ostringstream oss;
            oss << "📜 Ваша история оценок:\n";
            for (const auto& [movieId, score] : session.movieRatings) {
                auto movieOpt = ctx.getMovieById(movieId);
                std::string title = movieOpt ? movieOpt->title : ("Фильм #" + std::to_string(movieId));
                oss << "• " << title << " — " << score << "/10\n";
            }
            if (!strategy.empty()) {
                oss << "\n🎯 Активный режим рекомендаций: " << strategy << "\n";
            }
            oss << "\nИспользуйте меню ниже для дальнейших действий:";
            messenger->sendMessageWithKeyboard(
                session.chatId,
                oss.str(),
                KeyboardFactory::createMainMenuKeyboard());
        }
        return;
    }

    if (cb.data == "menu:help") {
        if (auto messenger = ctx.getMessenger()) {
            std::string helpText =
                "ℹ️ Справка по командам бота:\n"
                "• /start — Запуск онбординга и выбор любимых жанров\n"
                "• /recommend — Подбор персональных рекомендаций фильмов\n"
                "• /genres — Настройка и смена списка любимых жанров\n"
                "• /history — Просмотр истории оцененных фильмов\n"
                "• /similar — Поиск похожих фильмов во время просмотра\n"
                "• /help — Справка по всем возможностям бота\n\n"
                "Используйте кнопки меню ниже для быстрой навигации:";
            messenger->sendMessageWithKeyboard(
                session.chatId,
                helpText,
                KeyboardFactory::createMainMenuKeyboard());
        }
        return;
    }
}

} // namespace bot
