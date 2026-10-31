#include "bot/states/BrowsingRecommendationsState.h"
#include "bot/KeyboardFactory.h"
#include "bot/MovieCardFormatter.h"

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
        if (session.currentMovie) {
            session.markSkipped(session.currentMovie->id);
        }
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

    if (msg.text == "/similar") {
        if (!session.currentMovie) {
            if (auto messenger = ctx.getMessenger()) {
                messenger->sendMessage(session.chatId, "Сейчас не выбран ни один фильм для поиска похожих.");
            }
            return;
        }

        auto similar = ctx.fetchSimilarMovies(session.currentMovie->id, 3);
        auto messenger = ctx.getMessenger();
        if (!messenger) return;

        if (similar.empty()) {
            messenger->sendMessage(session.chatId, "К сожалению, похожих фильмов не найдено.");
        } else {
            std::ostringstream oss;
            oss << "🎬 Фильмы, похожие на «" << session.currentMovie->title << "»:\n\n";
            for (size_t i = 0; i < similar.size(); ++i) {
                oss << (i + 1) << ". " << similar[i].title;
                if (similar[i].year > 0) oss << " (" << similar[i].year << ")";
                if (!similar[i].director.empty()) oss << " — реж. " << similar[i].director;
                if (similar[i].ratingAvg > 0.0) oss << " [★ " << similar[i].ratingAvg << "/10]";
                oss << "\n";
            }
            messenger->sendMessage(session.chatId, oss.str());
        }
        return;
    }

    if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessage(session.chatId,
            "Используйте /rate для оценки фильма, /next для следующего, /similar для похожих, /genres для смены жанров или /menu для выхода.");
    }
}

void BrowsingRecommendationsState::handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) {
    auto parseCallback = [](const std::string& data, const std::string& prefix) -> std::pair<bool, int64_t> {
        if (data == prefix) {
            return {true, 0};
        }
        std::string fullPrefix = prefix + ":";
        if (data.rfind(fullPrefix, 0) == 0) {
            try {
                int64_t id = std::stoll(data.substr(fullPrefix.size()));
                return {true, id};
            } catch (...) {
                return {true, 0};
            }
        }
        return {false, 0};
    };

    auto resolveMovie = [&](int64_t movieId) -> std::optional<core::Movie> {
        if (movieId > 0) {
            auto m = ctx.getMovieById(movieId);
            if (m.has_value()) {
                return m;
            }
        }
        return session.currentMovie;
    };

    auto [matchedLike, likeMovieId] = parseCallback(cb.data, "rec:like");
    if (matchedLike) {
        auto targetMovie = resolveMovie(likeMovieId);
        if (targetMovie) {
            session.movieRatings[targetMovie->id] = 10;
            ctx.recordMovieRating(session.userId, targetMovie->id, 10);
            ctx.recordReaction(session.userId, targetMovie->id, true);
            session.lastActionNotice = "👍 Вы отметили фильм «" + targetMovie->title + "» как понравившийся (10/10)!";
        }
        showNextMovie(ctx, session);
        return;
    }

    auto [matchedDislike, dislikeMovieId] = parseCallback(cb.data, "rec:dislike");
    if (matchedDislike) {
        auto targetMovie = resolveMovie(dislikeMovieId);
        if (targetMovie) {
            session.movieRatings[targetMovie->id] = 1;
            ctx.recordMovieRating(session.userId, targetMovie->id, 1);
            ctx.recordReaction(session.userId, targetMovie->id, false);
            session.lastActionNotice = "👎 Вы отметили фильм «" + targetMovie->title + "» как непонравившийся (1/10).";
        }
        showNextMovie(ctx, session);
        return;
    }

    auto [matchedNext, nextMovieId] = parseCallback(cb.data, "rec:next");
    if (matchedNext) {
        auto targetMovie = resolveMovie(nextMovieId);
        if (targetMovie) {
            session.markSkipped(targetMovie->id);
            session.lastActionNotice = "⏭ Фильм «" + targetMovie->title + "» пропущен.";
        }
        showNextMovie(ctx, session);
        return;
    }

    auto [matchedRate, rateMovieId] = parseCallback(cb.data, "rec:rate");
    if (matchedRate) {
        auto targetMovie = resolveMovie(rateMovieId);
        if (targetMovie) {
            session.currentMovie = targetMovie;
        }
        ctx.transitionTo(session.userId, BotState::RatingMovie);
        return;
    }

    if (cb.data == "rec:menu") {
        ctx.transitionTo(session.userId, BotState::Idle);
        return;
    }

    auto [matchedSimilar, similarMovieId] = parseCallback(cb.data, "rec:similar");
    if (matchedSimilar) {
        auto targetMovie = resolveMovie(similarMovieId);
        if (!targetMovie) {
            if (auto messenger = ctx.getMessenger()) {
                messenger->sendMessage(session.chatId, "Сейчас не выбран ни один фильм для поиска похожих.");
            }
            return;
        }

        auto similar = ctx.fetchSimilarMovies(targetMovie->id, 3);
        auto messenger = ctx.getMessenger();
        if (!messenger) return;

        if (similar.empty()) {
            messenger->sendMessage(session.chatId, "К сожалению, похожих фильмов на «" + targetMovie->title + "» не найдено.");
        } else {
            std::ostringstream oss;
            oss << "🎬 Фильмы, похожие на «" << targetMovie->title << "»:\n\n";
            for (size_t i = 0; i < similar.size(); ++i) {
                oss << (i + 1) << ". " << similar[i].title;
                if (similar[i].year > 0) oss << " (" << similar[i].year << ")";
                if (!similar[i].director.empty()) oss << " — реж. " << similar[i].director;
                if (similar[i].ratingAvg > 0.0) oss << " [★ " << similar[i].ratingAvg << "/10]";
                oss << "\n";
            }
            messenger->sendMessage(session.chatId, oss.str());
        }
        return;
    }
}

void BrowsingRecommendationsState::showNextMovie(IDialogStateContext& ctx, UserSession& session) {
    auto messenger = ctx.getMessenger();
    auto nextMovie = ctx.fetchNextMovie(session.userId);

    if (!nextMovie) {
        if (messenger) {
            messenger->sendMessageWithKeyboard(session.chatId,
                "🎉 Вы просмотрели все доступные рекомендации по вашим жанрам!\n"
                "Вы можете изменить жанры (/genres) или вернуться в главное меню (/menu).",
                KeyboardFactory::createMainMenuKeyboard());
        }
        session.currentMovie = std::nullopt;
        ctx.transitionTo(session.userId, BotState::Idle);
        return;
    }

    session.currentMovie = nextMovie;
    ctx.recordImpression(session.userId, nextMovie->id);

    if (!messenger) {
        return;
    }

    InlineKeyboard keyboard = KeyboardFactory::createMovieReactionKeyboard(
        session.currentMovie->id,
        session.currentMovie->trailerUrl
    );
    std::string notice = session.lastActionNotice;
    session.lastActionNotice.clear();

    std::string explanation = ctx.buildRecommendationExplanation(session.userId, *session.currentMovie);
    size_t ratedCount = session.movieRatings.size();
    std::string userStatus;
    if (ratedCount > 0) {
        userStatus = "📊 Оценено вами фильмов: " + std::to_string(ratedCount);
    }

    MovieCardFormatter::sendMovieCard(
        *messenger,
        session.chatId,
        *session.currentMovie,
        keyboard,
        notice,
        explanation,
        userStatus,
        [&ctx](int64_t movieId, const std::string& fileId) {
            ctx.cachePosterFileId(movieId, fileId);
        });
}

} // namespace bot
