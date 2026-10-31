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
#include <vector>

/**
 * @file IDialogState.h
 * @brief Интерфейсы контекста конечного автомата (FSM) и состояний диалога Telegram-бота.
 */

namespace bot {

/**
 * @class IDialogStateContext
 * @brief Контракт контекста диалога, предоставляющий доступ к сессиям, мессенджеру и переходам состояний.
 *
 * Передается в обработчики состояний для выполнения переходов, отправки сообщений,
 * сохранения предпочтений и получения рекомендаций фильмов из сервиса рекомендаций.
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
     * @brief Сохраняет выбранные пользователем жанры в ядро/БД.
     * @param userId Идентификатор пользователя Telegram.
     * @param genres Список названий жанров.
     */
    virtual void saveUserGenres(int64_t userId, const std::vector<std::string>& genres) {
        (void)userId;
        (void)genres;
    }

    /**
     * @brief Сохраняет оценку фильма пользователем в ядро/БД.
     * @param userId Идентификатор пользователя Telegram.
     * @param movieId Идентификатор фильма.
     * @param score Оценка от 1 до 10.
     */
    virtual void recordMovieRating(int64_t userId, int64_t movieId, int score) {
        (void)userId;
        (void)movieId;
        (void)score;
    }

    /**
     * @brief Возвращает список похожих фильмов для указанного фильма.
     * @param movieId Идентификатор исходного фильма.
     * @param count Максимальное количество похожих фильмов.
     * @return Вектор похожих фильмов.
     */
    virtual std::vector<core::Movie> fetchSimilarMovies(int64_t movieId, size_t count = 3) {
        (void)movieId;
        (void)count;
        return {};
    }

    /**
     * @brief Возвращает название текущей активной стратегии рекомендаций для пользователя.
     * @param userId Идентификатор пользователя Telegram.
     * @return Название стратегии либо пустая строка.
     */
    [[nodiscard]] virtual std::string getActiveStrategyName(int64_t userId) const {
        (void)userId;
        return "";
    }

    /**
     * @brief Формирует понятное объяснение, почему фильм был предложен пользователю.
     * @param userId Идентификатор пользователя.
     * @param movie Рекомендуемый фильм.
     * @return Понятная строка с описанием причины рекомендации.
     */
    [[nodiscard]] virtual std::string buildRecommendationExplanation(int64_t userId, const core::Movie& movie) const {
        (void)userId;
        (void)movie;
        return "";
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

    /**
     * @brief Форматирует сводный текстовый отчет продуктовых метрик рекомендаций.
     * @return Строка отчета (CTR, DAU, задержка) либо пустая строка.
     */
    [[nodiscard]] virtual std::string getMetricsReport() const {
        return "";
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
     * @brief Обрабатывает входящий callback-запрос inline-кнопки в текущем состоянии.
     * @param ctx Контекст конечного автомата.
     * @param session Сессия текущего пользователя.
     * @param cb DTO входящего callback-запроса.
     */
    virtual void handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) = 0;
};

} // namespace bot
