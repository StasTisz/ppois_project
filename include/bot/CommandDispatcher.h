#pragma once

#include "bot/BotEvents.h"
#include "bot/IBotMessenger.h"
#include "bot/IDialogState.h"

#include <memory>
#include <string>

/**
 * @file CommandDispatcher.h
 * @brief Диспетчер глобальных команд бота (/start, /recommend, /genres, /history, /help).
 */

namespace bot {

/**
 * @class CommandDispatcher
 * @brief Обрабатывает глобальные команды пользователя независимо от текущего состояния FSM.
 */
class CommandDispatcher {
public:
    /**
     * @brief Конструктор диспетчера команд.
     * @param context Контекст конечного автомата диалогов.
     * @param messenger Клиент отправки сообщений в Telegram.
     */
    CommandDispatcher(IDialogStateContext& context, std::shared_ptr<IBotMessenger> messenger);

    /**
     * @brief Извлекает команду из строки, отсекая аргументы и суффикс бота (@botname).
     * @param text Текст входящего сообщения.
     * @return Имя команды (начиная с '/'), либо пустая строка.
     */
    [[nodiscard]] static std::string extractCommand(const std::string& text);

    /**
     * @brief Маршрутизирует команду, если она является глобальной.
     * @param msg DTO входящего текстового сообщения.
     * @return true, если команда распознана и обработана, иначе false.
     */
    bool dispatch(const IncomingMessage& msg);

    /**
     * @brief Форматирует список закладок («Буду смотреть») пользователя.
     * @param session Сессия пользователя.
     * @param ctx Контекст диалога.
     * @return Строка с отформатированным списком фильмов или сообщением о пустом списке.
     */
    [[nodiscard]] static std::string formatBookmarksText(
        const UserSession& session, const IDialogStateContext& ctx);

private:
    void handleStart(UserSession& session);
    void handleRecommend(UserSession& session);
    void handleGenres(UserSession& session);
    void handleHistory(const UserSession& session);
    void handleBookmarks(const UserSession& session);
    void handleStats(const UserSession& session);
    void handleHelp(const UserSession& session);

    [[nodiscard]] std::string formatHistoryText(const UserSession& session, const std::string& strategy) const;
    [[nodiscard]] static std::string buildHelpText();

    IDialogStateContext& context_;
    std::shared_ptr<IBotMessenger> messenger_;
};

} // namespace bot
