#include "bot/states/RatingState.h"
#include "bot/BotConstants.h"
#include "bot/KeyboardFactory.h"

#include <string>

/**
 * @file RatingState.cpp
 * @brief Реализация логики выставления оценки в состоянии BotState::RatingMovie.
 */

namespace bot {

BotState RatingState::getStateType() const {
    return BotState::RatingMovie;
}

void RatingState::onEnter(IDialogStateContext& ctx, UserSession& session) {
    auto messenger = ctx.getMessenger();
    if (!messenger) {
        return;
    }
    std::string movieTitle = session.hasCurrentMovie() ? session.getCurrentMovieTitle() : "текущий фильм";
    messenger->sendMessageWithKeyboard(
        session.chatId,
        "⭐ Оценка фильма «" + movieTitle + "»:\n"
        "Выберите оценку от 1 до 10 на клавиатуре ниже или отправьте число сообщением (либо /cancel):",
        KeyboardFactory::createRatingKeyboard());
}

std::optional<int> RatingState::parseRatingText(const std::string& text) {
    try {
        size_t processedChars = 0;
        int score = std::stoi(text, &processedChars);
        if (processedChars == text.size() && score >= kMinRating && score <= kMaxRating) {
            return score;
        }
    } catch (...) {}
    return std::nullopt;
}

void RatingState::applyRating(IDialogStateContext& ctx, UserSession& session, int score) {
    if (session.hasCurrentMovie()) {
        int64_t movieId = *session.getCurrentMovieId();
        session.recordRating(movieId, score);
        ctx.recordMovieRating(session.userId, movieId, score);
        ctx.recordReaction(session.userId, movieId, score >= kPositiveRatingThreshold);
    }
    if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessage(session.chatId, "✅ Оценка " + std::to_string(score) + "/10 успешно сохранена!");
    }
    ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
}

void RatingState::cancelRating(IDialogStateContext& ctx, UserSession& session) {
    if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessage(session.chatId, "Оценка отменена.");
    }
    ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
}

void RatingState::handleMessage(
    IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) {
    if (msg.text == "/cancel" || msg.text == "/back") {
        cancelRating(ctx, session);
        return;
    }

    auto scoreOpt = parseRatingText(msg.text);
    if (scoreOpt.has_value()) {
        applyRating(ctx, session, *scoreOpt);
        return;
    }

    if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessage(session.chatId,
            "⚠️ Некорректная оценка. Пожалуйста, введите целое число от 1 до 10 (или /cancel для отмены).");
    }
}

void RatingState::handleCallback(
    IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) {
    const std::string ratePrefix = "rate:";
    if (cb.data.rfind(ratePrefix, 0) != 0) {
        return;
    }

    std::string payload = cb.data.substr(ratePrefix.size());
    if (payload == "cancel") {
        cancelRating(ctx, session);
        return;
    }

    auto scoreOpt = parseRatingText(payload);
    if (scoreOpt.has_value()) {
        applyRating(ctx, session, *scoreOpt);
    }
}

} // namespace bot
