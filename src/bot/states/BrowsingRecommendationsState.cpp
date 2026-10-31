#include "bot/states/BrowsingRecommendationsState.h"
#include "bot/BotConstants.h"
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

void BrowsingRecommendationsState::handleMessage(
    IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) {
    if (msg.text == "/rate") {
        ctx.transitionTo(session.userId, BotState::RatingMovie);
    } else if (msg.text == "/next") {
        if (session.hasCurrentMovie()) {
            session.markSkipped(session.getCurrentMovie()->id);
        }
        showNextMovie(ctx, session);
    } else if (msg.text == "/genres") {
        ctx.transitionTo(session.userId, BotState::SelectingGenres);
    } else if (msg.text == "/menu" || msg.text == "/start") {
        ctx.transitionTo(session.userId, BotState::Idle);
    } else if (msg.text == "/similar") {
        handleSimilarCommand(ctx, session);
    } else if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessage(session.chatId,
            "Используйте /rate для оценки, /next для следующего, /similar для похожих, "
            "/genres для смены жанров или /menu для выхода.");
    }
}

void BrowsingRecommendationsState::handleSimilarCommand(IDialogStateContext& ctx, UserSession& session) {
    auto messenger = ctx.getMessenger();
    if (!messenger) {
        return;
    }
    if (!session.hasCurrentMovie()) {
        messenger->sendMessage(session.chatId, "Сейчас не выбран ни один фильм для поиска похожих.");
        return;
    }
    auto similar = ctx.fetchSimilarMovies(session.getCurrentMovie()->id, kDefaultSimilarMoviesLimit);
    messenger->sendMessage(session.chatId, formatSimilarMoviesText(session.getCurrentMovie()->title, similar));
}

std::string BrowsingRecommendationsState::formatSimilarMoviesText(
    const std::string& title, const std::vector<core::Movie>& similar) {
    if (similar.empty()) {
        return "К сожалению, похожих фильмов на «" + title + "» не найдено.";
    }
    std::ostringstream oss;
    oss << "🎬 Фильмы, похожие на «" << title << "»:\n\n";
    for (size_t i = 0; i < similar.size(); ++i) {
        oss << (i + 1) << ". " << similar[i].title;
        if (similar[i].year > 0) oss << " (" << similar[i].year << ")";
        if (!similar[i].director.empty()) oss << " — реж. " << similar[i].director;
        if (similar[i].ratingAvg > 0.0) oss << " [★ " << similar[i].ratingAvg << "/10]";
        oss << "\n";
    }
    return oss.str();
}

std::pair<bool, int64_t> BrowsingRecommendationsState::parseCallbackPayload(
    const std::string& data, const std::string& prefix) {
    if (data == prefix) {
        return {true, 0};
    }
    std::string fullPrefix = prefix + ":";
    if (data.rfind(fullPrefix, 0) == 0) {
        try {
            return {true, std::stoll(data.substr(fullPrefix.size()))};
        } catch (...) {
            return {true, 0};
        }
    }
    return {false, 0};
}

std::optional<core::Movie> BrowsingRecommendationsState::resolveTargetMovie(
    IDialogStateContext& ctx, const UserSession& session, int64_t movieId) const {
    if (movieId > 0) {
        auto m = ctx.getMovieById(movieId);
        if (m.has_value()) {
            return m;
        }
    }
    return session.getCurrentMovie();
}

void BrowsingRecommendationsState::handleLike(IDialogStateContext& ctx, UserSession& session, int64_t movieId) {
    auto target = resolveTargetMovie(ctx, session, movieId);
    if (target) {
        session.recordRating(target->id, kRatingLike);
        ctx.recordMovieRating(session.userId, target->id, kRatingLike);
        ctx.recordReaction(session.userId, target->id, true);
        session.setLastActionNotice("👍 Вы отметили фильм «" + target->title + "» как понравившийся (10/10)!");
    }
    showNextMovie(ctx, session);
}

void BrowsingRecommendationsState::handleDislike(IDialogStateContext& ctx, UserSession& session, int64_t movieId) {
    auto target = resolveTargetMovie(ctx, session, movieId);
    if (target) {
        session.recordRating(target->id, kRatingDislike);
        ctx.recordMovieRating(session.userId, target->id, kRatingDislike);
        ctx.recordReaction(session.userId, target->id, false);
        session.setLastActionNotice("👎 Вы отметили фильм «" + target->title + "» как непонравившийся (1/10).");
    }
    showNextMovie(ctx, session);
}

void BrowsingRecommendationsState::handleNext(IDialogStateContext& ctx, UserSession& session, int64_t movieId) {
    auto target = resolveTargetMovie(ctx, session, movieId);
    if (target) {
        session.markSkipped(target->id);
        session.setLastActionNotice("⏭ Фильм «" + target->title + "» пропущен.");
    }
    showNextMovie(ctx, session);
}

void BrowsingRecommendationsState::handleRate(IDialogStateContext& ctx, UserSession& session, int64_t movieId) {
    auto target = resolveTargetMovie(ctx, session, movieId);
    if (target) {
        session.setCurrentMovie(target);
    }
    ctx.transitionTo(session.userId, BotState::RatingMovie);
}

void BrowsingRecommendationsState::handleSimilarCallback(
    IDialogStateContext& ctx, UserSession& session, int64_t movieId) {
    auto target = resolveTargetMovie(ctx, session, movieId);
    auto messenger = ctx.getMessenger();
    if (!messenger) return;
    if (!target) {
        messenger->sendMessage(session.chatId, "Сейчас не выбран ни один фильм для поиска похожих.");
        return;
    }
    auto similar = ctx.fetchSimilarMovies(target->id, kDefaultSimilarMoviesLimit);
    messenger->sendMessage(session.chatId, formatSimilarMoviesText(target->title, similar));
}

void BrowsingRecommendationsState::handleCallback(
    IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) {
    if (auto [matched, id] = parseCallbackPayload(cb.data, "rec:like"); matched) {
        handleLike(ctx, session, id);
    } else if (auto [matchedDis, idDis] = parseCallbackPayload(cb.data, "rec:dislike"); matchedDis) {
        handleDislike(ctx, session, idDis);
    } else if (auto [matchedNext, idNext] = parseCallbackPayload(cb.data, "rec:next"); matchedNext) {
        handleNext(ctx, session, idNext);
    } else if (auto [matchedRate, idRate] = parseCallbackPayload(cb.data, "rec:rate"); matchedRate) {
        handleRate(ctx, session, idRate);
    } else if (cb.data == "rec:menu") {
        ctx.transitionTo(session.userId, BotState::Idle);
    } else if (auto [matchedSim, idSim] = parseCallbackPayload(cb.data, "rec:similar"); matchedSim) {
        handleSimilarCallback(ctx, session, idSim);
    }
}

void BrowsingRecommendationsState::handleNoMoreRecommendations(
    IDialogStateContext& ctx, UserSession& session) {
    if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessageWithKeyboard(session.chatId,
            "🎉 Вы просмотрели все доступные рекомендации по вашим жанрам!\n"
            "Вы можете изменить жанры (/genres) или вернуться в главное меню (/menu).",
            KeyboardFactory::createMainMenuKeyboard());
    }
    session.clearCurrentMovie();
    ctx.transitionTo(session.userId, BotState::Idle);
}

void BrowsingRecommendationsState::sendNextMovieCard(
    IDialogStateContext& ctx, UserSession& session, core::Movie& movie) {
    auto messenger = ctx.getMessenger();
    if (!messenger) return;
    auto keyboard = KeyboardFactory::createMovieReactionKeyboard(movie.id, movie.trailerUrl);
    std::string notice = session.consumeLastActionNotice();
    std::string explanation = ctx.buildRecommendationExplanation(session.userId, movie);
    size_t ratedCount = session.getRatingCount();
    std::string userStatus = (ratedCount > 0) ? ("📊 Оценено вами фильмов: " + std::to_string(ratedCount)) : "";
    MovieCardFormatter::sendMovieCard(
        *messenger, session.chatId, movie, keyboard, notice, explanation, userStatus,
        [&ctx](int64_t movieId, const std::string& fileId) { ctx.cachePosterFileId(movieId, fileId); });
}

void BrowsingRecommendationsState::showNextMovie(IDialogStateContext& ctx, UserSession& session) {
    auto nextMovie = ctx.fetchNextMovie(session.userId);
    if (!nextMovie) {
        handleNoMoreRecommendations(ctx, session);
        return;
    }
    session.setCurrentMovie(nextMovie);
    ctx.recordImpression(session.userId, nextMovie->id);
    sendNextMovieCard(ctx, session, *nextMovie);
}

} // namespace bot
