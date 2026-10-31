#pragma once

#include "bot/IDialogState.h"

#include <string>

/**
 * @file DefaultState.h
 * @brief Состояние главного меню Telegram-бота (соответствует BotState::Idle).
 */

namespace bot {

/**
 * @class DefaultState
 * @brief Обработчик состояния ожидания и главного меню бота.
 */
class DefaultState : public IDialogState {
public:
    [[nodiscard]] BotState getStateType() const override;

    void onEnter(IDialogStateContext& ctx, UserSession& session) override;

    void handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) override;

    void handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) override;

private:
    void handleRecommendCommand(IDialogStateContext& ctx, UserSession& session);
    void handleHelpCommand(IDialogStateContext& ctx, const UserSession& session);
    void handleHistoryCallback(IDialogStateContext& ctx, const UserSession& session);
    [[nodiscard]] std::string formatHistoryText(IDialogStateContext& ctx, const UserSession& session) const;
};

} // namespace bot
