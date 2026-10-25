#include "bot/states/RatingState.h"
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

    std::string movieTitle = session.currentMovie ? session.currentMovie->title : "текущий фильм";
    messenger->sendMessageWithKeyboard(session.chatId,
        "⭐ Оценка фильма «" + movieTitle + "»:\n"
        "Выберите оценку от 1 до 10 на клавиатуре ниже или отправьте число сообщением (либо /cancel):",
        KeyboardFactory::createRatingKeyboard());
}

void RatingState::handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) {
    auto messenger = ctx.getMessenger();

    if (msg.text == "/cancel" || msg.text == "/back") {
        if (messenger) {
            messenger->sendMessage(session.chatId, "Оценка отменена.");
        }
        ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
        return;
    }

    // Пробуем распарсить целое число от 1 до 10
    bool parsedSuccessfully = false;
    int score = 0;
    try {
        size_t processedChars = 0;
        score = std::stoi(msg.text, &processedChars);
        if (processedChars == msg.text.size() && score >= 1 && score <= 10) {
            parsedSuccessfully = true;
        }
    } catch (...) {
        parsedSuccessfully = false;
    }

    if (parsedSuccessfully) {
        if (session.currentMovie) {
            ctx.recordMovieRating(session.userId, session.currentMovie->id, score);
        }
        if (messenger) {
            messenger->sendMessage(session.chatId,
                "✅ Оценка " + std::to_string(score) + "/10 успешно сохранена!");
        }
        ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
        return;
    }

    if (messenger) {
        messenger->sendMessage(session.chatId,
            "⚠️ Некорректная оценка. Пожалуйста, введите целое число от 1 до 10 (или /cancel для отмены).");
    }
}

void RatingState::handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) {
    const std::string ratePrefix = "rate:";
    if (cb.data.rfind(ratePrefix, 0) == 0) {
        std::string payload = cb.data.substr(ratePrefix.size());

        if (payload == "cancel") {
            if (auto messenger = ctx.getMessenger()) {
                messenger->sendMessage(session.chatId, "Оценка отменена.");
            }
            ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
            return;
        }

        try {
            int score = std::stoi(payload);
            if (score >= 1 && score <= 10) {
                if (session.currentMovie) {
                    ctx.recordMovieRating(session.userId, session.currentMovie->id, score);
                }
                if (auto messenger = ctx.getMessenger()) {
                    messenger->sendMessage(session.chatId,
                        "✅ Оценка " + std::to_string(score) + "/10 успешно сохранена!");
                }
                ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
                return;
            }
        } catch (...) {
            // Игнорируем некорректный callback
        }
    }
}

} // namespace bot
