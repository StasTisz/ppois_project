#include "recommendation/RecommendationService.h"
#include "recommendation/PopularityRecommendationStrategy.h"
#include "recommendation/ContentBasedRecommendationStrategy.h"
#include <chrono>
#include <algorithm>

namespace recommendation {

RecommendationService::RecommendationService(
    std::shared_ptr<repository::IMovieRepository> movieRepo,
    std::shared_ptr<repository::IUserRepository> userRepo
)
    : movieRepo_(std::move(movieRepo)),
      userRepo_(std::move(userRepo)),
      popularityStrategy_(std::make_shared<PopularityRecommendationStrategy>(movieRepo_, userRepo_)),
      contentBasedStrategy_(std::make_shared<ContentBasedRecommendationStrategy>(movieRepo_, userRepo_)) {}

RecommendationService::RecommendationService(
    std::shared_ptr<repository::IMovieRepository> movieRepo,
    std::shared_ptr<repository::IUserRepository> userRepo,
    std::shared_ptr<IRecommendationStrategy> popularityStrategy,
    std::shared_ptr<IRecommendationStrategy> contentBasedStrategy
)
    : movieRepo_(std::move(movieRepo)),
      userRepo_(std::move(userRepo)),
      popularityStrategy_(std::move(popularityStrategy)),
      contentBasedStrategy_(std::move(contentBasedStrategy)) {}

bool RecommendationService::hasSufficientHistory(int64_t userId, int minPositiveRatings) const {
    if (!userRepo_) return false;

    const auto ratings = userRepo_->getUserRatings(userId);
    int positiveCount = 0;
    for (const auto& r : ratings) {
        if (r.score >= 7) {
            ++positiveCount;
            if (positiveCount >= minPositiveRatings) {
                return true;
            }
        }
    }
    return false;
}

std::shared_ptr<IRecommendationStrategy> RecommendationService::selectStrategy(
    int64_t userId,
    RecommendationMode mode
) const {
    if (customStrategy_) {
        return customStrategy_;
    }

    switch (mode) {
        case RecommendationMode::Popularity:
            return popularityStrategy_;

        case RecommendationMode::ContentBased:
            return contentBasedStrategy_;

        case RecommendationMode::Auto:
        default:
            // Auto-selection: If user has rated at least one movie positively (score >= 7),
            // use Content-Based strategy; otherwise fall back to Cold-Start / Popularity.
            if (hasSufficientHistory(userId, 1)) {
                return contentBasedStrategy_;
            }
            return popularityStrategy_;
    }
}

std::vector<core::Movie> RecommendationService::getRecommendations(
    int64_t userId,
    size_t count,
    RecommendationMode mode
) {
    if (count == 0) {
        return {};
    }

    auto strategy = selectStrategy(userId, mode);
    if (!strategy) {
        return {};
    }

    auto recommendations = strategy->recommend(userId, count);

    // Fallback: If content-based produced nothing (e.g. edge case), try popularity
    if (recommendations.empty() && mode == RecommendationMode::Auto && strategy != popularityStrategy_) {
        if (popularityStrategy_) {
            recommendations = popularityStrategy_->recommend(userId, count);
        }
    }

    return recommendations;
}

std::string RecommendationService::resolveStrategyName(
    int64_t userId,
    RecommendationMode mode
) const {
    auto strategy = selectStrategy(userId, mode);
    return strategy ? strategy->getName() : "None";
}

std::vector<core::Movie> RecommendationService::getSimilarMovies(int64_t movieId, size_t count) {
    if (count == 0 || !movieRepo_) {
        return {};
    }

    auto targetOpt = movieRepo_->getById(movieId);
    if (!targetOpt.has_value()) {
        return {};
    }

    const auto& targetMovie = targetOpt.value();
    const auto allMovies = movieRepo_->getAll();

    std::vector<std::pair<core::Movie, double>> scored;
    scored.reserve(allMovies.size());

    for (const auto& movie : allMovies) {
        if (movie.id == movieId) {
            continue; // Skip the movie itself
        }
        double sim = ContentBasedRecommendationStrategy::calculateMovieSimilarity(movie, targetMovie);
        scored.emplace_back(movie, sim);
    }

    std::sort(scored.begin(), scored.end(),
        [](const std::pair<core::Movie, double>& a, const std::pair<core::Movie, double>& b) {
            if (std::abs(a.second - b.second) > 0.0001) {
                return a.second > b.second;
            }
            return a.first.ratingAvg > b.first.ratingAvg;
        });

    std::vector<core::Movie> result;
    result.reserve(std::min(count, scored.size()));
    for (size_t i = 0; i < count && i < scored.size(); ++i) {
        result.push_back(scored[i].first);
    }

    return result;
}

bool RecommendationService::rateMovie(int64_t userId, int64_t movieId, int score) {
    if (score < 1 || score > 10 || !userRepo_ || !movieRepo_) {
        return false;
    }

    if (!movieRepo_->getById(movieId).has_value()) {
        return false; // Movie does not exist
    }

    auto now = std::chrono::system_clock::now();
    int64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();

    core::Rating rating{0, userId, movieId, score, timestamp};
    return userRepo_->addRating(rating);
}

bool RecommendationService::setFavoriteGenres(int64_t userId, const std::vector<std::string>& genres) {
    if (!userRepo_) return false;
    return userRepo_->setFavoriteGenres(userId, genres);
}

std::vector<std::string> RecommendationService::getFavoriteGenres(int64_t userId) const {
    if (!userRepo_) return {};
    return userRepo_->getFavoriteGenres(userId);
}

std::vector<core::Rating> RecommendationService::getUserRatings(int64_t userId) const {
    if (!userRepo_) return {};
    return userRepo_->getUserRatings(userId);
}

std::optional<core::Movie> RecommendationService::getMovieDetails(int64_t movieId) const {
    if (!movieRepo_) return std::nullopt;
    return movieRepo_->getById(movieId);
}

void RecommendationService::setCustomStrategy(std::shared_ptr<IRecommendationStrategy> strategy) {
    customStrategy_ = std::move(strategy);
}

void RecommendationService::clearCustomStrategy() {
    customStrategy_.reset();
}

} // namespace recommendation
