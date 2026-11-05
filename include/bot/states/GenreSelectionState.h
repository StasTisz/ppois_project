#pragma once

#include "bot/IDialogState.h"

#include <string>

/**
 * @file GenreSelectionState.h
 * @brief Состояние выбора любимых жанров пользователя (соответствует BotState::SelectingGenres).
 */

namespace bot {

/**
 * @class GenreSelectionState
 * @brief Обработчик выбора и переключения жанровых предпочтений пользователя.
 */
class GenreSelectionState : public IDialogState {
public:
    [[nodiscard]] BotState getStateType() const override;

    void onEnter(IDialogStateContext& ctx, UserSession& session) override;

    void handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) override;

    void handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) override;

private:
    void handleDone(IDialogStateContext& ctx, UserSession& session);
    void handleUnknownGenre(IDialogStateContext& ctx, UserSession& session, const std::string& text);
    void handleTextGenre(IDialogStateContext& ctx, UserSession& session, const std::string& text);
    void handleToggleCallback(
        IDialogStateContext& ctx, UserSession& session, const std::string& data, int32_t messageId);

    [[nodiscard]] static std::string buildStatusText(
        const UserSession& session, const std::string& prefix = "");
    static void sendStatusMessage(
        IDialogStateContext& ctx, const UserSession& session, const std::string& prefix = "");
};

} // namespace bot
