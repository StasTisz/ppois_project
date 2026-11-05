#pragma once

#include "recommendation/IRecommendationStrategy.h"
#include "repository/IMovieRepository.h"
#include "repository/IUserRepository.h"
#include <memory>

namespace recommendation {

/**
 * @brief Strategy for Cold-Start and Popularity-based movie recommendations.
 *
 * Recommends top-rated movies based on user's favorite genres. If the user has
 * no favorite genres or not enough movies match, it falls back to overall top-rated
 * movies. Automatically filters out already rated / watched movies.
 */
class PopularityRecommendationStrategy : public IRecommendationStrategy {
public:
    PopularityRecommendationStrategy(
        std::shared_ptr<repository::IMovieRepository> movieRepo,
        std::shared_ptr<repository::IUserRepository> userRepo
    );

    ~PopularityRecommendationStrategy() override = default;

    std::string getName() const override;

    std::vector<core::Movie> recommend(int64_t userId, size_t count) override;

private:
    std::shared_ptr<repository::IMovieRepository> movieRepo_;
    std::shared_ptr<repository::IUserRepository> userRepo_;
};

} // namespace recommendation
