#include "bot/states/BrowsingRecommendationsState.h"

#include <sstream>

/**
 * @file BrowsingRecommendationsState.cpp
 * @brief Реализация логики просмотра фильмов в состоянии BotState::BrowsingRecommendations.
 */

namespace bot {

BotState BrowsingRecommendationsState::getStateType() const {
    return BotState::BrowsingRecommendations;
}

void BrowsingRecommendationsState::onEnter(IDialogStateContext& ctx, UserSession& session) {
    showNextMovie(ctx, session);
}

void BrowsingRecommendationsState::handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) {
    if (msg.text == "/rate") {
        ctx.transitionTo(session.userId, BotState::RatingMovie);
        return;
    }

    if (msg.text == "/next") {
        showNextMovie(ctx, session);
        return;
    }

    if (msg.text == "/genres") {
        ctx.transitionTo(session.userId, BotState::SelectingGenres);
        return;
    }

    if (msg.text == "/menu" || msg.text == "/start") {
        ctx.transitionTo(session.userId, BotState::Idle);
        return;
    }

    if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessage(session.chatId,
            "Используйте /rate для оценки фильма, /next для следующего, /genres для смены жанров или /menu для выхода.");
    }
}

void BrowsingRecommendationsState::handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) {
    if (cb.data == "rec:like") {
        if (session.currentMovie) {
            session.movieRatings[session.currentMovie->id] = 10;
            if (auto messenger = ctx.getMessenger()) {
                messenger->sendMessage(session.chatId, "👍 Вы отметили фильм как понравившийся (10/10)!");
            }
        }
        showNextMovie(ctx, session);
        return;
    }

    if (cb.data == "rec:dislike") {
        if (session.currentMovie) {
            session.movieRatings[session.currentMovie->id] = 1;
            if (auto messenger = ctx.getMessenger()) {
                messenger->sendMessage(session.chatId, "👎 Вы отметили фильм как непонравившийся (1/10).");
            }
        }
        showNextMovie(ctx, session);
        return;
    }

    if (cb.data == "rec:next") {
        showNextMovie(ctx, session);
        return;
    }

    if (cb.data == "rec:rate") {
        ctx.transitionTo(session.userId, BotState::RatingMovie);
        return;
    }

    if (cb.data == "rec:menu") {
        ctx.transitionTo(session.userId, BotState::Idle);
    }
}

void BrowsingRecommendationsState::showNextMovie(IDialogStateContext& ctx, UserSession& session) {
    auto messenger = ctx.getMessenger();
    auto nextMovie = ctx.fetchNextMovie(session.userId);

    if (!nextMovie) {
        if (messenger) {
            messenger->sendMessage(session.chatId,
                "🎉 Вы просмотрели все доступные рекомендации по вашим жанрам!\n"
                "Вы можете изменить жанры (/genres) или вернуться в главное меню (/menu).");
        }
        session.currentMovie = std::nullopt;
        ctx.transitionTo(session.userId, BotState::Idle);
        return;
    }

    session.currentMovie = *nextMovie;

    if (messenger) {
        std::ostringstream oss;
        oss << "🎬 " << nextMovie->title << " (" << nextMovie->year << ")\n"
            << "⭐ Рейтинг: " << nextMovie->ratingAvg << " / 10\n"
            << "📁 Жанры: ";

        for (size_t i = 0; i < nextMovie->genres.size(); ++i) {
            oss << nextMovie->genres[i];
            if (i + 1 < nextMovie->genres.size()) {
                oss << ", ";
            }
        }
        oss << "\n";

        if (!nextMovie->director.empty()) {
            oss << "👤 Режиссер: " << nextMovie->director << "\n";
        }

        if (!nextMovie->description.empty()) {
            oss << "\n" << nextMovie->description << "\n";
        }

        oss << "\nРеакции: 👍 Понравился (rec:like) | 👎 Не понравился (rec:dislike) | ⏭ Следующий (rec:next) | ⭐ Оценить (rec:rate)";
        messenger->sendMessage(session.chatId, oss.str());
    }
}

} // namespace bot
