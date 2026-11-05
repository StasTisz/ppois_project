#pragma once

#include "bot/IDialogState.h"

#include <optional>
#include <string>

/**
 * @file RatingState.h
 * @brief Состояние выставления детальной оценки фильму (соответствует BotState::RatingMovie).
 */

namespace bot {

/**
 * @class RatingState
 * @brief Обработчик выставления оценки текущему фильму по шкале от 1 до 10.
 */
class RatingState : public IDialogState {
public:
    [[nodiscard]] BotState getStateType() const override;

    void onEnter(IDialogStateContext& ctx, UserSession& session) override;

    void handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) override;

    void handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) override;

private:
    [[nodiscard]] static std::optional<int> parseRatingText(const std::string& text);
    void applyRating(IDialogStateContext& ctx, UserSession& session, int score);
    void cancelRating(IDialogStateContext& ctx, UserSession& session);
};

} // namespace bot
