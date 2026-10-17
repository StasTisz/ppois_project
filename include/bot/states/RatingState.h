#pragma once

#include "bot/IDialogState.h"

/**
 * @file RatingState.h
 * @brief Состояние выставления детальной оценки фильму (соответствует BotState::RatingMovie).
 */

namespace bot {

/**
 * @class RatingState
 * @brief Обработчик выставления оценки текущему фильму по шкале от 1 до 10.
 *
 * Принимает оценку текстовым сообщением или callback-запросом кнопки (`rate:<1-10>`),
 * сохраняет её в сессию пользователя и возвращает в режим рекомендаций.
 */
class RatingState : public IDialogState {
public:
    /**
     * @brief Возвращает тип состояния BotState::RatingMovie.
     * @return BotState::RatingMovie.
     */
    [[nodiscard]] BotState getStateType() const override;

    /**
     * @brief Предлагает пользователю ввести оценку от 1 до 10 для текущего фильма.
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     */
    void onEnter(IDialogStateContext& ctx, UserSession& session) override;

    /**
     * @brief Обрабатывает ввод числа (1-10) либо команду отмены (/cancel).
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     * @param msg Входящее текстовое сообщение.
     */
    void handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) override;

    /**
     * @brief Обрабатывает callback-запрос оценки (например, rate:8 или rate:cancel).
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     * @param cb Входящий callback-запрос.
     */
    void handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) override;
};

} // namespace bot
