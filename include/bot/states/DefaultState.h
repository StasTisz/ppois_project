#pragma once

#include "bot/IDialogState.h"

/**
 * @file DefaultState.h
 * @brief Состояние главного меню Telegram-бота (соответствует BotState::Idle).
 */

namespace bot {

/**
 * @class DefaultState
 * @brief Обработчик состояния ожидания и главного меню бота.
 *
 * Обрабатывает команды навигации по боту (`/recommend`, `/genres`, `/help`)
 * и инициирует переходы в режим выбора жанров или просмотр рекомендаций.
 */
class DefaultState : public IDialogState {
public:
    /**
     * @brief Возвращает тип состояния BotState::Idle.
     * @return BotState::Idle.
     */
    [[nodiscard]] BotState getStateType() const override;

    /**
     * @brief Отправляет пользователю приветственное сообщение главного меню со списком команд.
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     */
    void onEnter(IDialogStateContext& ctx, UserSession& session) override;

    /**
     * @brief Обрабатывает текстовые команды пользователя в главном меню.
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     * @param msg Входящее текстовое сообщение.
     */
    void handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) override;

    /**
     * @brief Обрабатывает callback-запросы inline-кнопок главного меню.
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     * @param cb Входящий callback-запрос.
     */
    void handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) override;
};

} // namespace bot
