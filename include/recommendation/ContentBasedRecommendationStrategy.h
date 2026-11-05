#pragma once

#include "recommendation/IRecommendationStrategy.h"
#include "repository/IMovieRepository.h"
#include "repository/IUserRepository.h"
#include <memory>
#include <vector>
#include <string>

namespace recommendation {

/**
 * @brief Content-Based Recommendation Strategy.
 *
 * Recommends unrated movies that are most similar in content (genres, director, era)
 * to movies that the user has rated highly (score >= minScoreThreshold).
 * If user has no ratings, falls back to favorite genres or popular movies.
 */
class ContentBasedRecommendationStrategy : public IRecommendationStrategy {
public:
    explicit ContentBasedRecommendationStrategy(
        std::shared_ptr<repository::IMovieRepository> movieRepo,
        std::shared_ptr<repository::IUserRepository> userRepo,
        int minScoreThreshold = 7
    );

    ~ContentBasedRecommendationStrategy() override = default;

    std::string getName() const override;

    std::vector<core::Movie> recommend(int64_t userId, size_t count) override;

    // Public similarity functions for transparency and testing
    static double calculateJaccardSimilarity(
        const std::vector<std::string>& genresA,
        const std::vector<std::string>& genresB
    );

    static double calculateMovieSimilarity(
        const core::Movie& a,
        const core::Movie& b
    );

private:
    std::shared_ptr<repository::IMovieRepository> movieRepo_;
    std::shared_ptr<repository::IUserRepository> userRepo_;
    int minScoreThreshold_{7};
};

} // namespace recommendation
