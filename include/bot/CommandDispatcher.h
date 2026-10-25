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
 *
 * Распознает и выполняет команды:
 * - `/start`: запуск онбординга и переход к выбору жанров.
 * - `/recommend`: запуск подбора рекомендаций или перенаправление на выбор жанров.
 * - `/genres`: переход к панели настройки предпочтений.
 * - `/history`: вывод истории оцененных фильмов пользователя.
 * - `/help`: подробная справка по функционалу бота.
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

private:
    void handleStart(UserSession& session);
    void handleRecommend(UserSession& session);
    void handleGenres(UserSession& session);
    void handleHistory(const UserSession& session);
    void handleHelp(const UserSession& session);

    IDialogStateContext& context_;
    std::shared_ptr<IBotMessenger> messenger_;
};

} // namespace bot
