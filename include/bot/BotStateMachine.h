#pragma once

#include "bot/BotEvents.h"
#include "bot/BotEventRouter.h"
#include "bot/BotState.h"
#include "bot/CommandDispatcher.h"
#include "bot/IDialogState.h"
#include "bot/UserSession.h"
#include "core/Movie.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @file BotStateMachine.h
 * @brief Менеджер конечного автомата диалогов (FSM) Telegram-бота.
 */

namespace bot {

/**
 * @class BotStateMachine
 * @brief Управляет сессиями пользователей, регистрацией состояний и переключениями FSM.
 *
 * Реализует `IDialogStateContext`, хранит изолированные сессии каждого пользователя,
 * диспетчеризирует текстовые сообщения и callback-запросы в активные состояния,
 * а также предоставляет временный пул рекомендаций до интеграции с базой данных.
 */
class BotStateMachine : public IDialogStateContext {
public:
    /// Сигнатура функции-поставщика фильмов (для кастомизации рекомендаций).
    using MovieProvider = std::function<std::optional<core::Movie>(int64_t)>;

    /**
     * @brief Конструктор автомата состояний. Инициализирует стандартные состояния и демо-фильмы.
     * @param messenger Умный указатель на IBotMessenger для отправки ответов в Telegram.
     */
    explicit BotStateMachine(std::shared_ptr<IBotMessenger> messenger);

    /**
     * @brief Деструктор автомата состояний.
     */
    ~BotStateMachine() override;

    /**
     * @brief Регистрирует состояние в карте состояний автомата.
     * @param state Умный указатель на реализацию IDialogState.
     */
    void registerState(std::shared_ptr<IDialogState> state);

    /**
     * @brief Получает существующую сессию пользователя или создает новую.
     * @param userId Идентификатор пользователя Telegram.
     * @param chatId Идентификатор чата (применяется при создании новой сессии).
     * @param username Никнейм пользователя.
     * @return Ссылка на UserSession пользователя.
     */
    UserSession& getSession(int64_t userId, int64_t chatId = 0, const std::string& username = "") override;

    /**
     * @brief Выполняет переход пользователя в новое состояние и вызывает onEnter.
     * @param userId Идентификатор пользователя.
     * @param newState Целевое состояние BotState.
     */
    void transitionTo(int64_t userId, BotState newState) override;

    /**
     * @brief Возвращает клиент мессенджера.
     * @return Умный указатель на IBotMessenger.
     */
    [[nodiscard]] std::shared_ptr<IBotMessenger> getMessenger() const override;

    /**
     * @brief Получает следующий фильм для показа пользователю (из кастомного провайдера либо fallback-пула).
     * @param userId Идентификатор пользователя Telegram.
     * @return Объект Movie либо std::nullopt.
     */
    std::optional<core::Movie> fetchNextMovie(int64_t userId) override;

    /**
     * @brief Находит фильм по его идентификатору в каталоге фильмов.
     * @param movieId Идентификатор фильма.
     * @return Объект Movie либо std::nullopt.
     */
    [[nodiscard]] std::optional<core::Movie> getMovieById(int64_t movieId) const override;

    /**
     * @brief Предоставляет доступ к диспетчеру глобальных команд.
     * @return Ссылка на объект CommandDispatcher.
     */
    [[nodiscard]] CommandDispatcher& getCommandDispatcher();

    /**
     * @brief Устанавливает пользовательский провайдер фильмов (для тестов и интеграции с сервисом рекомендаций).
     * @param provider Функция-поставщик фильмов.
     */
    void setMovieProvider(MovieProvider provider);

    /**
     * @brief Обрабатывает входящее текстовое сообщение.
     * При глобальной команде `/start` переводит в BotState::SelectingGenres, иначе делегирует текущему состоянию.
     * @param msg Входящее текстовое сообщение.
     */
    void processMessage(const IncomingMessage& msg);

    /**
     * @brief Обрабатывает входящий callback-запрос кнопки, делегируя текущему состоянию.
     * @param cb Входящий callback-запрос.
     */
    void processCallback(const IncomingCallback& cb);

    /**
     * @brief Связывает обработку сообщений и callback-запросов автомата с маршрутизатором BotEventRouter.
     * @param router Маршрутизатор событий бота.
     */
    void attachToRouter(BotEventRouter& router);

private:
    /**
     * @brief Инициализирует стандартные состояния автомата.
     */
    void registerDefaultStates();

    /**
     * @brief Заполняет демо-список фильмов для автономной работы до подключения SQLite.
     */
    void initFallbackMovies();

    std::shared_ptr<IBotMessenger> messenger_;
    std::unique_ptr<CommandDispatcher> commandDispatcher_;
    std::unordered_map<BotState, std::shared_ptr<IDialogState>> states_;
    std::unordered_map<int64_t, UserSession> sessions_;
    std::vector<core::Movie> fallbackMovies_;
    MovieProvider movieProvider_;
};

} // namespace bot
