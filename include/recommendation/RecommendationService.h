#pragma once

#include "recommendation/IRecommendationStrategy.h"
#include "repository/IMovieRepository.h"
#include "repository/IUserRepository.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace recommendation {

enum class RecommendationMode {
    Auto,         // Automatically selects Strategy based on user history depth
    Popularity,   // Forces Popularity / Cold-Start strategy
    ContentBased  // Forces Content-Based Filtering strategy
};

inline std::string recommendationModeToString(RecommendationMode mode) {
    switch (mode) {
        case RecommendationMode::Auto: return "Auto";
        case RecommendationMode::Popularity: return "Popularity";
        case RecommendationMode::ContentBased: return "ContentBased";
        default: return "Unknown";
    }
}

/**
 * @brief Facade service for client and bot layers.
 *
 * Encapsulates repositories, strategy selection, profile analysis,
 * rating operations, and similar movie search behind a unified, easy-to-use API.
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
        std::shared_ptr<IRecommendationStrategy> contentBasedStrategy
    );

    virtual ~RecommendationService() = default;

    // Recommendation facade
    std::vector<core::Movie> getRecommendations(
        int64_t userId,
        size_t count = 5,
        RecommendationMode mode = RecommendationMode::Auto
    );

    std::string resolveStrategyName(
        int64_t userId,
        RecommendationMode mode = RecommendationMode::Auto
    ) const;

    // Find movies similar to a specific movie (e.g. for "More like this" bot button)
    std::vector<core::Movie> getSimilarMovies(int64_t movieId, size_t count = 5);

    // Profile & user actions
    bool rateMovie(int64_t userId, int64_t movieId, int score);
    bool setFavoriteGenres(int64_t userId, const std::vector<std::string>& genres);
    std::vector<std::string> getFavoriteGenres(int64_t userId) const;
    std::vector<core::Rating> getUserRatings(int64_t userId) const;
    std::optional<core::Movie> getMovieDetails(int64_t movieId) const;

    // Strategy management
    void setCustomStrategy(std::shared_ptr<IRecommendationStrategy> strategy);
    void clearCustomStrategy();

    // Check if user has enough history for content-based recommendations
    bool hasSufficientHistory(int64_t userId, int minPositiveRatings = 1) const;

private:
    std::shared_ptr<repository::IMovieRepository> movieRepo_;
    std::shared_ptr<repository::IUserRepository> userRepo_;
    std::shared_ptr<IRecommendationStrategy> popularityStrategy_;
    std::shared_ptr<IRecommendationStrategy> contentBasedStrategy_;
    std::shared_ptr<IRecommendationStrategy> customStrategy_;

    std::shared_ptr<IRecommendationStrategy> selectStrategy(int64_t userId, RecommendationMode mode) const;
};

} // namespace recommendation
