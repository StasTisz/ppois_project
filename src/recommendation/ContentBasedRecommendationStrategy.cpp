#include "recommendation/ContentBasedRecommendationStrategy.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace recommendation {

ContentBasedRecommendationStrategy::ContentBasedRecommendationStrategy(
    std::shared_ptr<repository::IMovieRepository> movieRepo,
    std::shared_ptr<repository::IUserRepository> userRepo,
    int minScoreThreshold
)
    : movieRepo_(std::move(movieRepo)),
      userRepo_(std::move(userRepo)),
      minScoreThreshold_(minScoreThreshold) {}

std::string ContentBasedRecommendationStrategy::getName() const {
    return "Content-Based Filtering";
}

double ContentBasedRecommendationStrategy::calculateJaccardSimilarity(
    const std::vector<std::string>& genresA,
    const std::vector<std::string>& genresB
) {
    if (genresA.empty() || genresB.empty()) {
        return 0.0;
    }

    std::unordered_set<std::string> setA(genresA.begin(), genresA.end());
    std::unordered_set<std::string> setB(genresB.begin(), genresB.end());

    size_t intersectionCount = 0;
    for (const auto& g : setA) {
        if (setB.find(g) != setB.end()) {
            ++intersectionCount;
        }
    }

    size_t unionCount = setA.size() + setB.size() - intersectionCount;
    if (unionCount == 0) {
        return 0.0;
    }

    return static_cast<double>(intersectionCount) / static_cast<double>(unionCount);
}

double ContentBasedRecommendationStrategy::calculateMovieSimilarity(
    const core::Movie& a,
    const core::Movie& b
) {
    // 1. Genre similarity (weight: 0.70)
    // Используем сверхбыстрый побитовый Жаккар (Zero-Allocation micro-embeddings)
    uint32_t maskA = a.getOrComputeGenreMask();
    uint32_t maskB = b.getOrComputeGenreMask();

    double genreSim = 0.0;
    if (maskA != 0 && maskB != 0) {
        genreSim = core::calculateJaccardBitwise(maskA, maskB);
    } else {
        genreSim = calculateJaccardSimilarity(a.genres, b.genres);
    }

    // 2. Director similarity (weight: 0.25)
    double directorSim = 0.0;
    if (!a.director.empty() && !b.director.empty() && a.director == b.director) {
        directorSim = 1.0;
    }

    // 3. Era / Decade similarity (weight: 0.05)
    double eraSim = 0.0;
    if (a.year > 0 && b.year > 0 && std::abs(a.year - b.year) <= 5) {
        eraSim = 1.0;
    }

    return (genreSim * 0.70) + (directorSim * 0.25) + (eraSim * 0.05);
}

std::vector<core::Movie> ContentBasedRecommendationStrategy::recommend(int64_t userId, size_t count) {
    if (count == 0 || !movieRepo_ || !userRepo_) {
        return {};
    }

    // 1. Collect movies the user already rated/watched to exclude
    const auto ratedIds = userRepo_->getRatedMovieIds(userId);
    const std::unordered_set<int64_t> excludedIds(ratedIds.begin(), ratedIds.end());

    // 2. Collect positive seed movies rated >= minScoreThreshold_
    const auto userRatings = userRepo_->getUserRatings(userId);
    std::vector<std::pair<core::Movie, double>> seeds;

    for (const auto& r : userRatings) {
        if (r.score >= minScoreThreshold_) {
            auto movieOpt = movieRepo_->getById(r.movieId);
            if (movieOpt.has_value()) {
                double weight = static_cast<double>(r.score) / 10.0;
                seeds.emplace_back(std::move(movieOpt.value()), weight);
            }
        }
    }

    // 3. Fallback: if user has no positive ratings, check favorite genres
    if (seeds.empty()) {
        const auto favGenres = userRepo_->getFavoriteGenres(userId);
        if (!favGenres.empty()) {
            core::Movie virtualSeed;
            virtualSeed.id = -1;
            virtualSeed.genres = favGenres;
            virtualSeed.year = 2015;
            seeds.emplace_back(std::move(virtualSeed), 1.0);
        }
    }

    // 4. If still no seeds at all, fallback to overall top-rated unrated movies
    if (seeds.empty()) {
        const size_t topQueryLimit = (count + excludedIds.size()) * 2 + 10;
        auto topMovies = movieRepo_->getTopRated(topQueryLimit);
        std::vector<core::Movie> fallbackRecs;
        for (const auto& m : topMovies) {
            if (excludedIds.find(m.id) == excludedIds.end()) {
                fallbackRecs.push_back(m);
                if (fallbackRecs.size() >= count) break;
            }
        }
        return fallbackRecs;
    }

    // 5. Score all unrated catalog movies against seeds
    const auto allMovies = movieRepo_->getAll();
    std::vector<std::pair<core::Movie, double>> scoredCandidates;
    scoredCandidates.reserve(allMovies.size());

    for (const auto& movie : allMovies) {
        if (excludedIds.find(movie.id) != excludedIds.end()) {
            continue; // Skip already rated movies
        }

        double totalSimilarity = 0.0;
        for (const auto& [seed, weight] : seeds) {
            double sim = calculateMovieSimilarity(movie, seed);
            totalSimilarity += sim * weight;
        }

        // Quality bias: slight bonus for overall rating to break ties
        double qualityBonus = (movie.ratingAvg / 10.0) * 0.15;
        double finalScore = totalSimilarity + qualityBonus;

        scoredCandidates.emplace_back(movie, finalScore);
    }

    // 6. Sort by final content score descending
    std::sort(scoredCandidates.begin(), scoredCandidates.end(),
        [](const std::pair<core::Movie, double>& a, const std::pair<core::Movie, double>& b) {
            if (std::abs(a.second - b.second) > 0.0001) {
                return a.second > b.second;
            }
            return a.first.ratingAvg > b.first.ratingAvg;
        });

    // 7. Extract top 'count' recommendations
    std::vector<core::Movie> recommendations;
    recommendations.reserve(std::min(count, scoredCandidates.size()));

    for (size_t i = 0; i < count && i < scoredCandidates.size(); ++i) {
        recommendations.push_back(scoredCandidates[i].first);
    }

    return recommendations;
}

} // namespace recommendation
