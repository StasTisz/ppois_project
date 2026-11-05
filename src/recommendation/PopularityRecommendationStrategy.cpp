#include "recommendation/PopularityRecommendationStrategy.h"
#include <algorithm>
#include <unordered_set>

namespace recommendation {

PopularityRecommendationStrategy::PopularityRecommendationStrategy(
    std::shared_ptr<repository::IMovieRepository> movieRepo,
    std::shared_ptr<repository::IUserRepository> userRepo
)
    : movieRepo_(std::move(movieRepo)), userRepo_(std::move(userRepo)) {}

std::string PopularityRecommendationStrategy::getName() const {
    return "Cold-Start / Popularity";
}

std::vector<core::Movie> PopularityRecommendationStrategy::recommend(int64_t userId, size_t count) {
    if (count == 0 || !movieRepo_ || !userRepo_) {
        return {};
    }

    // 1. Get already rated/viewed movies to exclude
    const auto ratedIds = userRepo_->getRatedMovieIds(userId);
    const std::unordered_set<int64_t> excludedIds(ratedIds.begin(), ratedIds.end());

    // 2. Get user's favorite genres
    const auto favoriteGenres = userRepo_->getFavoriteGenres(userId);

    std::vector<core::Movie> candidates;
    std::unordered_set<int64_t> candidateIds;

    // 3. If user has chosen favorite genres, prioritize top movies from those genres
    if (!favoriteGenres.empty()) {
        const size_t perGenreLimit = count * 2;
        for (const auto& genre : favoriteGenres) {
            auto genreMovies = movieRepo_->getByGenre(genre, perGenreLimit);
            for (const auto& movie : genreMovies) {
                if (excludedIds.find(movie.id) == excludedIds.end() &&
                    candidateIds.find(movie.id) == candidateIds.end()) {
                    candidateIds.insert(movie.id);
                    candidates.push_back(movie);
                }
            }
        }
    }

    // 4. If candidates are fewer than requested count, backfill with overall top-rated movies
    if (candidates.size() < count) {
        const size_t needed = count - candidates.size();
        const size_t topQueryLimit = (needed + excludedIds.size()) * 2 + 10;
        auto topMovies = movieRepo_->getTopRated(topQueryLimit);

        for (const auto& movie : topMovies) {
            if (excludedIds.find(movie.id) == excludedIds.end() &&
                candidateIds.find(movie.id) == candidateIds.end()) {
                candidateIds.insert(movie.id);
                candidates.push_back(movie);
                if (candidates.size() >= count) {
                    break;
                }
            }
        }
    }

    // 5. Sort candidates by rating descending (secondary sort: year descending)
    std::sort(candidates.begin(), candidates.end(), [](const core::Movie& a, const core::Movie& b) {
        if (a.ratingAvg != b.ratingAvg) {
            return a.ratingAvg > b.ratingAvg;
        }
        return a.year > b.year;
    });

    // 6. Truncate to the requested count
    if (candidates.size() > count) {
        candidates.resize(count);
    }

    return candidates;
}

} // namespace recommendation
