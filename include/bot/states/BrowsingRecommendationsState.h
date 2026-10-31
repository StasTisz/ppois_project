#pragma once

#include "bot/IDialogState.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

/**
 * @file BrowsingRecommendationsState.h
 * @brief Состояние просмотра рекомендаций фильмов (соответствует BotState::BrowsingRecommendations).
 */

namespace bot {

/**
 * @class BrowsingRecommendationsState
 * @brief Обработчик сценария последовательного просмотра рекомендованных фильмов.
 */
class BrowsingRecommendationsState : public IDialogState {
public:
    [[nodiscard]] BotState getStateType() const override;

    void onEnter(IDialogStateContext& ctx, UserSession& session) override;

    void handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) override;

    void handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) override;

private:
    void showNextMovie(IDialogStateContext& ctx, UserSession& session);
    void handleNoMoreRecommendations(IDialogStateContext& ctx, UserSession& session);
    void sendNextMovieCard(IDialogStateContext& ctx, UserSession& session, core::Movie& movie);
    void handleSimilarCommand(IDialogStateContext& ctx, UserSession& session);
    void handleLike(IDialogStateContext& ctx, UserSession& session, int64_t movieId);
    void handleDislike(IDialogStateContext& ctx, UserSession& session, int64_t movieId);
    void handleNext(IDialogStateContext& ctx, UserSession& session, int64_t movieId);
    void handleRate(IDialogStateContext& ctx, UserSession& session, int64_t movieId);
    void handleSimilarCallback(IDialogStateContext& ctx, UserSession& session, int64_t movieId);

    [[nodiscard]] std::optional<core::Movie> resolveTargetMovie(
        IDialogStateContext& ctx, const UserSession& session, int64_t movieId) const;

    [[nodiscard]] static std::string formatSimilarMoviesText(
        const std::string& title, const std::vector<core::Movie>& similar);

    [[nodiscard]] static std::pair<bool, int64_t> parseCallbackPayload(
        const std::string& data, const std::string& prefix);
};

} // namespace bot
