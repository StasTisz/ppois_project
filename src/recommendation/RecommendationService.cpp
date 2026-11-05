#include "recommendation/RecommendationService.h"
#include "recommendation/PopularityRecommendationStrategy.h"
#include "recommendation/ContentBasedRecommendationStrategy.h"
#include "recommendation/CollaborativeFilteringStrategy.h"
#include "recommendation/HybridRecommendationStrategy.h"

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
      contentBasedStrategy_(std::make_shared<ContentBasedRecommendationStrategy>(movieRepo_, userRepo_)),
      collaborativeStrategy_(std::make_shared<CollaborativeFilteringStrategy>(movieRepo_, userRepo_)),
      hybridStrategy_(std::make_shared<HybridRecommendationStrategy>(
          movieRepo_, userRepo_, popularityStrategy_, contentBasedStrategy_, collaborativeStrategy_, 0.15)),
      metricsCollector_(std::make_shared<analytics::MetricsCollector>()) {}

RecommendationService::RecommendationService(
    std::shared_ptr<repository::IMovieRepository> movieRepo,
    std::shared_ptr<repository::IUserRepository> userRepo,
    std::shared_ptr<IRecommendationStrategy> popularityStrategy,
    std::shared_ptr<IRecommendationStrategy> contentBasedStrategy,
    std::shared_ptr<IRecommendationStrategy> collaborativeStrategy,
    std::shared_ptr<IRecommendationStrategy> hybridStrategy
)
    : movieRepo_(std::move(movieRepo)),
      userRepo_(std::move(userRepo)),
      popularityStrategy_(std::move(popularityStrategy)),
      contentBasedStrategy_(std::move(contentBasedStrategy)),
      collaborativeStrategy_(collaborativeStrategy
          ? std::move(collaborativeStrategy)
          : std::make_shared<CollaborativeFilteringStrategy>(movieRepo_, userRepo_)),
      hybridStrategy_(hybridStrategy
          ? std::move(hybridStrategy)
          : std::make_shared<HybridRecommendationStrategy>(
              movieRepo_, userRepo_, popularityStrategy_, contentBasedStrategy_, collaborativeStrategy_, 0.15)),
      metricsCollector_(std::make_shared<analytics::MetricsCollector>()) {}

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

        case RecommendationMode::Collaborative:
            return collaborativeStrategy_;

        case RecommendationMode::Hybrid:
            return hybridStrategy_;

        case RecommendationMode::Auto:
        default:
            // Auto-selection:
            // 1. Зрелый профиль (>= 3 положительных оценок): гибридная фильтрация с Exploration/Exploitation
            // 2. Начальный профиль (1-2 положительные оценки): контентная фильтрация
            // 3. Холодный старт (0 оценок): стратегия популярности
            if (hasSufficientHistory(userId, 3) && hybridStrategy_) {
                return hybridStrategy_;
            }
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

    auto start = std::chrono::steady_clock::now();

    auto strategy = selectStrategy(userId, mode);
    if (!strategy) {
        return {};
    }

    auto recommendations = strategy->recommend(userId, count);

    // Fallback: если выбранная стратегия ничего не вернула, используем популярные
    if (recommendations.empty() && mode == RecommendationMode::Auto && strategy != popularityStrategy_) {
        if (popularityStrategy_) {
            recommendations = popularityStrategy_->recommend(userId, count);
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start);

    if (metricsCollector_) {
        metricsCollector_->recordRecommendationLatency(elapsed);
        metricsCollector_->recordUserActivity(userId);
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

std::vector<core::Movie> RecommendationService::searchMovies(const std::string& query, size_t limit) const {
    if (!movieRepo_ || query.empty()) {
        return {};
    }
    return movieRepo_->findByTitle(query, limit);
}

void RecommendationService::setExplorationRate(double rate) {
    explorationRate_ = std::clamp(rate, 0.0, 1.0);
    if (auto hybrid = std::dynamic_pointer_cast<HybridRecommendationStrategy>(hybridStrategy_)) {
        hybrid->setExplorationRate(explorationRate_);
    }
}

double RecommendationService::getExplorationRate() const {
    return explorationRate_;
}

std::shared_ptr<analytics::MetricsCollector> RecommendationService::getMetricsCollector() const {
    return metricsCollector_;
}

void RecommendationService::setMetricsCollector(std::shared_ptr<analytics::MetricsCollector> collector) {
    metricsCollector_ = std::move(collector);
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
            continue; // Пропускаем сам целевой фильм
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
        return false; // Фильм не найден в каталоге
    }

    auto now = std::chrono::system_clock::now();
    int64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();

    core::Rating rating{0, userId, movieId, score, timestamp};
    bool ok = userRepo_->addRating(rating);
    if (ok && metricsCollector_) {
        if (score >= 7) {
            metricsCollector_->recordLike(userId, movieId);
        } else {
            metricsCollector_->recordDislike(userId, movieId);
        }
    }
    return ok;
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

std::shared_ptr<IRecommendationStrategy> RecommendationService::getPopularityStrategy() const {
    return popularityStrategy_;
}

std::shared_ptr<IRecommendationStrategy> RecommendationService::getContentBasedStrategy() const {
    return contentBasedStrategy_;
}

std::shared_ptr<IRecommendationStrategy> RecommendationService::getCollaborativeStrategy() const {
    return collaborativeStrategy_;
}

std::shared_ptr<IRecommendationStrategy> RecommendationService::getHybridStrategy() const {
    return hybridStrategy_;
}

} // namespace recommendation
