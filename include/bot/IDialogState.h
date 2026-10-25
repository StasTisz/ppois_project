#pragma once

#include "bot/BotEvents.h"
#include "bot/BotState.h"
#include "bot/IBotMessenger.h"
#include "bot/UserSession.h"
#include "core/Movie.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

/**
 * @file IDialogState.h
 * @brief Интерфейсы контекста конечного автомата (FSM) и состояний диалога Telegram-бота.
 */

namespace bot {

/**
 * @class IDialogStateContext
 * @brief Контракт контекста диалога, предоставляющий доступ к сессиям, мессенджеру и переходам состояний.
 *
 * Передается в обработчики состояний для выполнения переходов, отправки сообщений
 * и получения рекомендаций фильмов.
 */
class IDialogStateContext {
public:
    virtual ~IDialogStateContext() = default;

    /**
     * @brief Получает существующую сессию пользователя или создает новую при её отсутствии.
     * @param userId Идентификатор пользователя Telegram.
     * @param chatId Опциональный идентификатор чата (для инициализации новой сессии).
     * @param username Опциональный юзернейм пользователя.
     * @return Ссылка на объект UserSession.
     */
    virtual UserSession& getSession(int64_t userId, int64_t chatId = 0, const std::string& username = "") = 0;

    /**
     * @brief Выполняет переход пользователя в новое состояние автомата.
     * @param userId Идентификатор пользователя Telegram.
     * @param newState Новое целевое состояние BotState.
     */
    virtual void transitionTo(int64_t userId, BotState newState) = 0;

    /**
     * @brief Предоставляет клиент для отправки сообщений в Telegram.
     * @return Умный указатель на IBotMessenger.
     */
    [[nodiscard]] virtual std::shared_ptr<IBotMessenger> getMessenger() const = 0;

    /**
     * @brief Получает следующий рекомендованный фильм для пользователя.
     * @param userId Идентификатор пользователя Telegram.
     * @return Рекомендованный фильм либо std::nullopt, если фильмы закончились.
     */
    virtual std::optional<core::Movie> fetchNextMovie(int64_t userId) = 0;

    /**
     * @brief Находит фильм по его идентификатору.
     * @param movieId Идентификатор фильма.
     * @return Объект Movie либо std::nullopt, если фильм не найден.
     */
    [[nodiscard]] virtual std::optional<core::Movie> getMovieById(int64_t movieId) const {
        (void)movieId;
        return std::nullopt;
    }

    /**
     * @brief Возвращает идентификатор чата-хранилища для предварительного кэширования медиа.
     * @return ID чата Telegram (0, если хранилище не настроено).
     */
    [[nodiscard]] virtual int64_t getStorageChatId() const {
        return 0;
    }

    /**
     * @brief Кэширует полученный от Telegram file_id постера в постоянное хранилище.
     * @param movieId Идентификатор фильма.
     * @param fileId Строковый идентификатор медиа Telegram.
     */
    virtual void cachePosterFileId(int64_t movieId, const std::string& fileId) {
        (void)movieId;
        (void)fileId;
    }

    virtual void recordImpression(int64_t userId, int64_t movieId) {
        (void)userId;
        (void)movieId;
    }

    virtual void recordReaction(int64_t userId, int64_t movieId, bool isLike) {
        (void)userId;
        (void)movieId;
        (void)isLike;
    }
};

/**
 * @class IDialogState
 * @brief Базовый интерфейс состояния диалогового конечного автомата (паттерн State).
 *
 * Каждое конкретное состояние инкапсулирует сценарий взаимодействия с пользователем,
 * реакцию на входящие текстовые команды и нажатия inline-кнопок.
 */
class IDialogState {
public:
    virtual ~IDialogState() = default;

    /**
     * @brief Возвращает тип состояния, соответствующий данному классу.
     * @return Значение перечисления BotState.
     */
    [[nodiscard]] virtual BotState getStateType() const = 0;

    /**
     * @brief Вызывается при входе пользователя в данное состояние.
     * @param ctx Контекст конечного автомата.
     * @param session Сессия текущего пользователя.
     */
    virtual void onEnter(IDialogStateContext& ctx, UserSession& session) = 0;

    /**
     * @brief Обрабатывает входящее текстовое сообщение пользователя в текущем состоянии.
     * @param ctx Контекст конечного автомата.
     * @param session Сессия текущего пользователя.
     * @param msg DTO входящего текстового сообщения.
     */
    virtual void handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) = 0;

    /**
     * @brief Обрабатывает входящий callback-запрос от нажатой inline-кнопки.
     * @param ctx Контекст конечного автомата.
     * @param session Сессия текущего пользователя.
     * @param cb DTO входящего callback-запроса.
     */
    virtual void handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) = 0;
};

} // namespace bot
