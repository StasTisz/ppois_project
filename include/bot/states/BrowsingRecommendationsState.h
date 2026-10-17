#pragma once

#include "bot/IDialogState.h"

/**
 * @file BrowsingRecommendationsState.h
 * @brief Состояние просмотра рекомендаций фильмов (соответствует BotState::BrowsingRecommendations).
 */

namespace bot {

/**
 * @class BrowsingRecommendationsState
 * @brief Обработчик сценария последовательного просмотра рекомендованных фильмов.
 *
 * Отображает карточку текущего фильма, реагирует на быстрые реакции
 * (`👍 Like`, `👎 Dislike`, `⏭ Next`), инициирует переход к детальной оценке
 * либо возврат в меню.
 */
class BrowsingRecommendationsState : public IDialogState {
public:
    /**
     * @brief Возвращает тип состояния BotState::BrowsingRecommendations.
     * @return BotState::BrowsingRecommendations.
     */
    [[nodiscard]] BotState getStateType() const override;

    /**
     * @brief Запрашивает следующий фильм и отправляет его карточку пользователю.
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     */
    void onEnter(IDialogStateContext& ctx, UserSession& session) override;

    /**
     * @brief Обрабатывает текстовые команды (/rate, /next, /genres, /menu, /start).
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     * @param msg Входящее текстовое сообщение.
     */
    void handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) override;

    /**
     * @brief Обрабатывает callback-запросы реакций (rec:like, rec:dislike, rec:next, rec:rate, rec:menu).
     * @param ctx Контекст конечного автомата диалогов.
     * @param session Сессия текущего пользователя.
     * @param cb Входящий callback-запрос.
     */
    void handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) override;

private:
    /**
     * @brief Загружает следующий фильм из контекста и отображает его карточку.
     * @param ctx Контекст конечного автомата.
     * @param session Сессия текущего пользователя.
     */
    void showNextMovie(IDialogStateContext& ctx, UserSession& session);
};

} // namespace bot
