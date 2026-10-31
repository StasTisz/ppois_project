#include "analytics/MetricsCollector.h"
#pragma once

#include "bot/BotEvents.h"
#include "bot/BotEventRouter.h"
#include "bot/BotState.h"
#include "bot/CommandDispatcher.h"
#include "bot/IDialogState.h"
#include "bot/UserSession.h"
#include "core/Movie.h"
#include "recommendation/RecommendationService.h"
#include "repository/IUserRepository.h"

#include <functional>
#include <mutex>
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
 * @brief Управляет сессиями пользователей, регистрацией состояний, переключениями FSM
 * и интеграцией с сервисом рекомендаций и базой данных SQLite.
 *
 * Реализует `IDialogStateContext`, хранит изолированные сессии каждого пользователя,
 * синхронизирует состояние пользователей и их оценки с базой данных SQLite,
 * диспетчеризирует текстовые сообщения и callback-запросы в активные состояния FSM.
 */
class BotStateMachine : public IDialogStateContext {
public:
    /// Сигнатура функции-поставщика фильмов (для кастомизации рекомендаций).
    using MovieProvider = std::function<std::optional<core::Movie>(int64_t)>;
    using PosterCacheCallback = std::function<void(int64_t, const std::string&)>;

    /**
     * @brief Конструктор автомата состояний. Инициализирует стандартные состояния и демо-фильмы.
     * @param messenger Умный указатель на IBotMessenger для отправки ответов в Telegram.
     */
    explicit BotStateMachine(std::shared_ptr<IBotMessenger> messenger);

    /**
     * @brief Конструктор с подключением сервиса рекомендаций и репозитория пользователей.
     * @param messenger Клиент отправки сообщений Telegram.
     * @param recService Сервис-фасад рекомендаций фильмов.
     * @param userRepo Репозиторий пользователей для синхронизации состояний диалога в SQLite.
     */
    BotStateMachine(
        std::shared_ptr<IBotMessenger> messenger,
        std::shared_ptr<recommendation::RecommendationService> recService,
        std::shared_ptr<repository::IUserRepository> userRepo = nullptr);

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
     * @brief Получает существующую сессию пользователя или создает новую с загрузкой профиля из БД.
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
     * @brief Получает следующий фильм для показа пользователю (из кастомного провайдера, сервиса рекомендаций либо fallback-пула).
     * @param userId Идентификатор пользователя Telegram.
     * @return Объект Movie либо std::nullopt.
     */
    std::optional<core::Movie> fetchNextMovie(int64_t userId) override;

    /**
     * @brief Находит фильм по его идентификатору в RecommendationService или fallback-каталоге.
     * @param movieId Идентификатор фильма.
     * @return Объект Movie либо std::nullopt.
     */
    [[nodiscard]] std::optional<core::Movie> getMovieById(int64_t movieId) const override;

    /**
     * @brief Сохраняет выбранные пользователем жанры в сессии и передает их в RecommendationService.
     * @param userId Идентификатор пользователя Telegram.
     * @param genres Список выбранных жанров.
     */
    void saveUserGenres(int64_t userId, const std::vector<std::string>& genres) override;

    /**
     * @brief Записывает оценку фильма в сессии и сохраняет в БД через RecommendationService.
     * @param userId Идентификатор пользователя Telegram.
     * @param movieId Идентификатор фильма.
     * @param score Оценка от 1 до 10.
     */
    void recordMovieRating(int64_t userId, int64_t movieId, int score) override;

    /**
     * @brief Возвращает похожие фильмы из RecommendationService.
     * @param movieId Идентификатор фильма.
     * @param count Количество фильмов.
     * @return Список похожих фильмов.
     */
    std::vector<core::Movie> fetchSimilarMovies(int64_t movieId, size_t count = 3) override;

    /**
     * @brief Возвращает название текущей стратегии рекомендаций.
     * @param userId Идентификатор пользователя Telegram.
     * @return Название стратегии.
     */
    [[nodiscard]] std::string getActiveStrategyName(int64_t userId) const override;

    /**
     * @brief Формирует понятное объяснение причины рекомендации фильма.
     * @param userId Идентификатор пользователя.
     * @param movie Рекомендуемый фильм.
     * @return Поясняющая строка алгоритма.
     */
    [[nodiscard]] std::string buildRecommendationExplanation(int64_t userId, const core::Movie& movie) const override;

    /**
     * @brief Предоставляет доступ к диспетчеру глобальных команд.
     * @return Ссылка на объект CommandDispatcher.
     */
    [[nodiscard]] CommandDispatcher& getCommandDispatcher();

    /**
     * @brief Устанавливает сервис рекомендаций и репозиторий пользователей.
     * @param recService Сервис рекомендаций.
     * @param userRepo Репозиторий пользователей.
     */
    void setRecommendationService(
        std::shared_ptr<recommendation::RecommendationService> recService,
        std::shared_ptr<repository::IUserRepository> userRepo = nullptr);

    /**
     * @brief Устанавливает пользовательский провайдер фильмов (для тестов и моков).
     * @param provider Функция-поставщик фильмов.
     */
    void setMovieProvider(MovieProvider provider);
    void setStorageChatId(int64_t chatId) { storageChatId_ = chatId; }
    [[nodiscard]] int64_t getStorageChatId() const override { return storageChatId_; }
    void setPosterCacheCallback(PosterCacheCallback callback) {
        std::lock_guard<std::mutex> lock(globalConfigMutex_);
        posterCacheCallback_ = std::move(callback);
    }
    void cachePosterFileId(int64_t movieId, const std::string& fileId) override;
    void setMetricsCollector(std::shared_ptr<analytics::MetricsCollector> collector) {
        std::lock_guard<std::mutex> lock(globalConfigMutex_);
        metricsCollector_ = std::move(collector);
    }
    [[nodiscard]] std::shared_ptr<analytics::MetricsCollector> getMetricsCollector() const {
        std::lock_guard<std::mutex> lock(globalConfigMutex_);
        return metricsCollector_;
    }
    void recordImpression(int64_t userId, int64_t movieId) override {
        std::shared_ptr<analytics::MetricsCollector> collector;
        {
            std::lock_guard<std::mutex> lock(globalConfigMutex_);
            collector = metricsCollector_;
        }
        if (collector) {
            collector->recordImpression(userId, movieId);
        }
    }
    void recordReaction(int64_t userId, int64_t movieId, bool isLike) override {
        std::shared_ptr<analytics::MetricsCollector> collector;
        {
            std::lock_guard<std::mutex> lock(globalConfigMutex_);
            collector = metricsCollector_;
        }
        if (collector) {
            if (isLike) collector->recordLike(userId, movieId);
            else collector->recordDislike(userId, movieId);
        }
    }

    /**
     * @brief Обрабатывает входящее текстовое сообщение.
     * При глобальных командах диспетчера обрабатывает их, иначе делегирует текущему состоянию.
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
     * @brief Заполняет демо-список фильмов для автономной работы без подключенной БД.
     */
    void initFallbackMovies();

    /**
     * @brief Возвращает рекурсивный мьютекс для синхронизации действий конкретного пользователя.
     * @param userId Идентификатор пользователя Telegram.
     */
    [[nodiscard]] std::shared_ptr<std::recursive_mutex> getUserMutex(int64_t userId) const;

    std::shared_ptr<IBotMessenger> messenger_;
    std::shared_ptr<recommendation::RecommendationService> recService_;
    std::shared_ptr<repository::IUserRepository> userRepo_;
    std::unique_ptr<CommandDispatcher> commandDispatcher_;
    std::unordered_map<BotState, std::shared_ptr<IDialogState>> states_;
    std::unordered_map<int64_t, UserSession> sessions_;
    std::vector<core::Movie> fallbackMovies_;
    MovieProvider movieProvider_;
    mutable std::mutex sessionMapMutex_;
    mutable std::mutex userMutexMapLock_;
    mutable std::unordered_map<int64_t, std::shared_ptr<std::recursive_mutex>> userMutexes_;
    mutable std::mutex globalConfigMutex_;
    int64_t storageChatId_{0};
    PosterCacheCallback posterCacheCallback_;
    std::shared_ptr<analytics::MetricsCollector> metricsCollector_;
};

} // namespace bot
