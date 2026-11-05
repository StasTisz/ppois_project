#pragma once

#include "analytics/MetricsCollector.h"
#include "recommendation/IRecommendationStrategy.h"
#include "repository/IMovieRepository.h"
#include "repository/IUserRepository.h"

#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace recommendation {

enum class RecommendationMode {
    Auto,          // Автоматический выбор: Hybrid (или Popularity при отсутствии истории)
    Popularity,    // Чистая стратегия популярности / холодного старта
    ContentBased,  // Контентная фильтрация по жанрам и предпочтениям
    Collaborative, // Коллаборативная фильтрация по похожим пользователям
    Hybrid         // Взвешенная гибридная фильтрация с Exploration/Exploitation
};

inline std::string recommendationModeToString(RecommendationMode mode) {
    switch (mode) {
        case RecommendationMode::Auto: return "Auto";
        case RecommendationMode::Popularity: return "Popularity";
        case RecommendationMode::ContentBased: return "ContentBased";
        case RecommendationMode::Collaborative: return "Collaborative";
        case RecommendationMode::Hybrid: return "Hybrid";
        default: return "Unknown";
    }
}

/**
 * @brief Фасад сервиса рекомендаций и аналитики для уровней приложения и Telegram-бота.
 *
 * Инкапсулирует стратегии (Popularity, ContentBased, Collaborative, Hybrid),
 * поиск фильмов по названию, работу с профилем, балансировку Exploration vs Exploitation
 * и сбор продуктовых метрик (CTR, DAU, Latency).
 */
class RecommendationService {
public:
    RecommendationService(
        std::shared_ptr<repository::IMovieRepository> movieRepo,
        std::shared_ptr<repository::IUserRepository> userRepo
    );

    RecommendationService(
        std::shared_ptr<repository::IMovieRepository> movieRepo,
        std::shared_ptr<repository::IUserRepository> userRepo,
        std::shared_ptr<IRecommendationStrategy> popularityStrategy,
        std::shared_ptr<IRecommendationStrategy> contentBasedStrategy,
        std::shared_ptr<IRecommendationStrategy> collaborativeStrategy = nullptr,
        std::shared_ptr<IRecommendationStrategy> hybridStrategy = nullptr
    );

    virtual ~RecommendationService() = default;

    // Рекомендательный фасад
    std::vector<core::Movie> getRecommendations(
        int64_t userId,
        size_t count = 5,
        RecommendationMode mode = RecommendationMode::Auto
    );

    // Удобный алиас
    std::vector<core::Movie> recommend(
        int64_t userId,
        size_t count = 5,
        RecommendationMode mode = RecommendationMode::Auto
    ) {
        return getRecommendations(userId, count, mode);
    }

    [[nodiscard]] std::string resolveStrategyName(
        int64_t userId,
        RecommendationMode mode = RecommendationMode::Auto
    ) const;

    // Поиск фильма по названию (для команды поиска в Telegram)
    [[nodiscard]] std::vector<core::Movie> searchMovies(const std::string& query, size_t limit = 10) const;

    // Поиск похожих фильмов ("Еще похожие")
    std::vector<core::Movie> getSimilarMovies(int64_t movieId, size_t count = 5);

    // Управление балансом Exploration vs Exploitation
    void setExplorationRate(double rate);
    [[nodiscard]] double getExplorationRate() const;

    // Доступ к коллектору метрик (CTR, DAU, Latency)
    [[nodiscard]] std::shared_ptr<analytics::MetricsCollector> getMetricsCollector() const;
    void setMetricsCollector(std::shared_ptr<analytics::MetricsCollector> collector);

    // Профиль и действия пользователя
    bool rateMovie(int64_t userId, int64_t movieId, int score);
    bool setFavoriteGenres(int64_t userId, const std::vector<std::string>& genres);
    std::vector<std::string> getFavoriteGenres(int64_t userId) const;
    std::vector<core::Rating> getUserRatings(int64_t userId) const;
    std::optional<core::Movie> getMovieDetails(int64_t movieId) const;

    // Управление стратегиями
    void setCustomStrategy(std::shared_ptr<IRecommendationStrategy> strategy);
    void clearCustomStrategy();

    [[nodiscard]] std::shared_ptr<IRecommendationStrategy> getPopularityStrategy() const;
    [[nodiscard]] std::shared_ptr<IRecommendationStrategy> getContentBasedStrategy() const;
    [[nodiscard]] std::shared_ptr<IRecommendationStrategy> getCollaborativeStrategy() const;
    [[nodiscard]] std::shared_ptr<IRecommendationStrategy> getHybridStrategy() const;

    bool hasSufficientHistory(int64_t userId, int minPositiveRatings = 1) const;

private:
    std::shared_ptr<repository::IMovieRepository> movieRepo_;
    std::shared_ptr<repository::IUserRepository> userRepo_;
    std::shared_ptr<IRecommendationStrategy> popularityStrategy_;
    std::shared_ptr<IRecommendationStrategy> contentBasedStrategy_;
    std::shared_ptr<IRecommendationStrategy> collaborativeStrategy_;
    std::shared_ptr<IRecommendationStrategy> hybridStrategy_;
    std::shared_ptr<IRecommendationStrategy> customStrategy_;
    std::shared_ptr<analytics::MetricsCollector> metricsCollector_;

    double explorationRate_{0.15};

    [[nodiscard]] std::shared_ptr<IRecommendationStrategy> selectStrategy(int64_t userId, RecommendationMode mode) const;
};

} // namespace recommendation
