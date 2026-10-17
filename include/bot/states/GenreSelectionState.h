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
 *
 * Позволяет отмечать и снимать жанры по callback-запросам (`genre:toggle:<genre>`)
 * или прямым текстовым вводом. По нажатию подтверждения (`genre:confirm`)
 * переводит пользователя в режим рекомендаций.
 */
class GenreSelectionState : public IDialogState {
public:
    /**
     * @brief Возвращает тип состояния BotState::SelectingGenres.
     * @return BotState::SelectingGenres.
     */
    [[nodiscard]] BotState getStateType() const override;

    /**
     * @brief Отправляет приглашение выбрать жанры и список текущих выбранных позиций.
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     */
    void onEnter(IDialogStateContext& ctx, UserSession& session) override;

    /**
     * @brief Обрабатывает текстовые сообщения (ввод названия жанра, команды /done, /cancel, /menu).
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     * @param msg Входящее текстовое сообщение.
     */
    void handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) override;

    /**
     * @brief Обрабатывает переключение жанров и подтверждение выбора по кнопкам.
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     * @param cb Входящий callback-запрос.
     */
    void handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) override;

private:
    /**
     * @brief Вспомогательный метод формирования и отправки статуса выбранных жанров.
     * @param ctx Контекст конечного автомата.
     * @param session Сессия текущего пользователя.
     * @param prefix Опциональный текст уведомления перед статусом.
     */
    static void sendStatusMessage(IDialogStateContext& ctx, const UserSession& session, const std::string& prefix = "");
};

} // namespace bot
