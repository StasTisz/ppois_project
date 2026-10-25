#include "recommendation/CollaborativeFilteringStrategy.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace recommendation {

CollaborativeFilteringStrategy::CollaborativeFilteringStrategy(
    std::shared_ptr<repository::IMovieRepository> movieRepo,
    std::shared_ptr<repository::IUserRepository> userRepo,
    size_t minCommonRatings
) : movieRepo_(std::move(movieRepo)),
    userRepo_(std::move(userRepo)),
    minCommonRatings_(minCommonRatings) {}

std::string CollaborativeFilteringStrategy::getName() const {
    return "CollaborativeFiltering";
}

double CollaborativeFilteringStrategy::calculateCosineSimilarity(
    const std::unordered_map<int64_t, int>& ratingsA,
    const std::unordered_map<int64_t, int>& ratingsB
) const {
    if (ratingsA.empty() || ratingsB.empty()) {
        return 0.0;
    }

    double dotProduct = 0.0;
    size_t commonCount = 0;

    for (const auto& [movieId, scoreA] : ratingsA) {
        auto it = ratingsB.find(movieId);
        if (it != ratingsB.end()) {
            dotProduct += static_cast<double>(scoreA * it->second);
            ++commonCount;
        }
    }

    if (commonCount < minCommonRatings_) {
        return 0.0;
    }

    double normA = 0.0;
    for (const auto& [_, score] : ratingsA) {
        normA += static_cast<double>(score * score);
    }

    double normB = 0.0;
    for (const auto& [_, score] : ratingsB) {
        normB += static_cast<double>(score * score);
    }

    if (normA <= 0.0 || normB <= 0.0) {
        return 0.0;
    }

    return dotProduct / (std::sqrt(normA) * std::sqrt(normB));
}

std::vector<core::Movie> CollaborativeFilteringStrategy::recommend(int64_t userId, size_t count) {
    std::vector<core::Movie> result;
    if (!movieRepo_ || !userRepo_) {
        return result;
    }

    // 1. Получаем оценки целевого пользователя
    auto myRatingsList = userRepo_->getUserRatings(userId);
    if (myRatingsList.empty()) {
        return result; // Холодный старт: нет оценок для коллаборативного сходства
    }

    std::unordered_map<int64_t, int> myRatings;
    std::unordered_set<int64_t> ratedMovieIds;
    for (const auto& r : myRatingsList) {
        myRatings[r.movieId] = r.score;
        ratedMovieIds.insert(r.movieId);
    }

    // 2. Получаем оценки всех остальных пользователей системы
    auto allRatings = userRepo_->getAllRatings();
    std::unordered_map<int64_t, std::unordered_map<int64_t, int>> userRatingProfiles;
    for (const auto& r : allRatings) {
        if (r.userId != userId) {
            userRatingProfiles[r.userId][r.movieId] = r.score;
        }
    }

    if (userRatingProfiles.empty()) {
        return result;
    }

    // 3. Вычисляем сходство со всеми соседями
    std::vector<std::pair<int64_t, double>> similarUsers;
    for (const auto& [otherUserId, otherRatings] : userRatingProfiles) {
        double sim = calculateCosineSimilarity(myRatings, otherRatings);
        if (sim > 0.01) { // Учитываем только положительно скоррелированных пользователей
            similarUsers.emplace_back(otherUserId, sim);
        }
    }

    if (similarUsers.empty()) {
        return result;
    }

    // 4. Прогнозируем рейтинг для непросмотренных фильмов
    // score_predicted(i) = sum(sim * score) / sum(sim)
    std::unordered_map<int64_t, double> weightedScores;
    std::unordered_map<int64_t, double> simSums;

    for (const auto& [otherUserId, sim] : similarUsers) {
        const auto& otherRatings = userRatingProfiles[otherUserId];
        for (const auto& [movieId, otherScore] : otherRatings) {
            // Пропускаем уже оцененные целевым пользователем фильмы
            if (ratedMovieIds.find(movieId) != ratedMovieIds.end()) {
                continue;
            }
            weightedScores[movieId] += sim * static_cast<double>(otherScore);
            simSums[movieId] += sim;
        }
    }

    // 5. Ранжируем кандидатов по прогнозируемой оценке
    std::vector<std::pair<int64_t, double>> rankedCandidates;
    for (const auto& [movieId, totalScore] : weightedScores) {
        double simSum = simSums[movieId];
        if (simSum > 0.0) {
            double predictedRating = totalScore / simSum;
            rankedCandidates.emplace_back(movieId, predictedRating);
        }
    }

    std::sort(rankedCandidates.begin(), rankedCandidates.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    // 6. Формируем итоговый список фильмов
    for (const auto& [movieId, _] : rankedCandidates) {
        if (result.size() >= count) {
            break;
        }
        auto movieOpt = movieRepo_->getById(movieId);
        if (movieOpt.has_value()) {
            result.push_back(*movieOpt);
        }
    }

    return result;
}

} // namespace recommendation
