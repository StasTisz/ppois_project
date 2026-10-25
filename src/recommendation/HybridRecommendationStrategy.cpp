#include "recommendation/HybridRecommendationStrategy.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace recommendation {

HybridRecommendationStrategy::HybridRecommendationStrategy(
    std::shared_ptr<repository::IMovieRepository> movieRepo,
    std::shared_ptr<repository::IUserRepository> userRepo,
    std::shared_ptr<IRecommendationStrategy> popularityStrategy,
    std::shared_ptr<IRecommendationStrategy> contentStrategy,
    std::shared_ptr<IRecommendationStrategy> collabStrategy,
    double explorationRate
) : movieRepo_(std::move(movieRepo)),
    userRepo_(std::move(userRepo)),
    popularityStrategy_(std::move(popularityStrategy)),
    contentStrategy_(std::move(contentStrategy)),
    collabStrategy_(std::move(collabStrategy)),
    explorationRate_(std::clamp(explorationRate, 0.0, 1.0)) {}

std::string HybridRecommendationStrategy::getName() const {
    return "HybridRecommendation";
}

void HybridRecommendationStrategy::setExplorationRate(double rate) {
    explorationRate_ = std::clamp(rate, 0.0, 1.0);
}

double HybridRecommendationStrategy::getExplorationRate() const {
    return explorationRate_;
}

std::vector<core::Movie> HybridRecommendationStrategy::recommend(int64_t userId, size_t count) {
    std::vector<core::Movie> result;
    if (count == 0 || !movieRepo_ || !userRepo_) {
        return result;
    }

    auto ratedIds = userRepo_->getRatedMovieIds(userId);
    std::unordered_set<int64_t> ratedSet(ratedIds.begin(), ratedIds.end());
    size_t ratingsCount = ratedIds.size();

    // 1. Динамическое распределение весов на основе зрелости профиля (Решение Cold Start)
    double wPop = 0.10;
    double wContent = 0.45;
    double wCollab = 0.45;

    if (ratingsCount == 0) {
        wPop = 0.75;
        wContent = 0.25;
        wCollab = 0.00;
    } else if (ratingsCount <= 2) {
        wPop = 0.20;
        wContent = 0.55;
        wCollab = 0.25;
    }

    size_t poolSize = std::max(count * 3, size_t(15));
    std::unordered_map<int64_t, double> compositeScores;
    std::unordered_map<int64_t, core::Movie> movieCache;

    auto accumulateStrategyScores = [&](const std::shared_ptr<IRecommendationStrategy>& strategy, double weight) {
        if (!strategy || weight <= 0.0) {
            return;
        }
        auto candidates = strategy->recommend(userId, poolSize);
        for (size_t rank = 0; rank < candidates.size(); ++rank) {
            const auto& m = candidates[rank];
            if (ratedSet.find(m.id) != ratedSet.end()) {
                continue;
            }
            // Ранговая нормализация: Reciprocal Rank Score
            double rankScore = 1.0 / (static_cast<double>(rank) + 1.0);
            compositeScores[m.id] += weight * rankScore;
            movieCache.try_emplace(m.id, m);
        }
    };

    accumulateStrategyScores(popularityStrategy_, wPop);
    accumulateStrategyScores(contentStrategy_, wContent);
    accumulateStrategyScores(collabStrategy_, wCollab);

    if (compositeScores.empty()) {
        // Fallback на топ по популярности
        if (popularityStrategy_) {
            return popularityStrategy_->recommend(userId, count);
        }
        return result;
    }

    // 2. Сортируем кандидатов по итоговому комбинированному баллу (Exploitation)
    std::vector<std::pair<int64_t, double>> rankedList(compositeScores.begin(), compositeScores.end());
    std::sort(rankedList.begin(), rankedList.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    // 3. Баланс Exploration vs Exploitation
    // Определяем количество слотов для исследования нового контента (Exploration)
    size_t explorationSlots = 0;
    if (explorationRate_ > 0.0 && count > 1) {
        explorationSlots = static_cast<size_t>(std::round(static_cast<double>(count) * explorationRate_));
        if (explorationSlots >= count) {
            explorationSlots = count - 1;
        }
    }
    size_t exploitationSlots = count - explorationSlots;

    std::unordered_set<int64_t> selectedIds;

    // Заполняем слоты эксплуатации (наиболее релевантные фильмы)
    for (const auto& [movieId, _] : rankedList) {
        if (result.size() >= exploitationSlots) {
            break;
        }
        result.push_back(movieCache[movieId]);
        selectedIds.insert(movieId);
    }

    // Заполняем слоты исследования (Exploration - фильмы с высоким общим качеством, но неожиданных жанров)
    if (explorationSlots > 0) {
        // Собираем уже представленные в выборке жанровые маски
        uint32_t chosenGenresMask = 0;
        for (const auto& m : result) {
            chosenGenresMask |= m.getOrComputeGenreMask();
        }

        // Ищем в пуле кандидатов фильм с новым/редким жанром для пользователя
        std::vector<core::Movie> explorationCandidates;
        for (const auto& [movieId, _] : rankedList) {
            if (selectedIds.find(movieId) != selectedIds.end()) {
                continue;
            }
            const auto& candidate = movieCache[movieId];
            uint32_t cMask = candidate.getOrComputeGenreMask();
            // Если у фильма есть жанры, не входящие в текущую эксплуатационную выборку
            if ((cMask & ~chosenGenresMask) != 0 || (chosenGenresMask == 0)) {
                explorationCandidates.push_back(candidate);
            }
        }

        // Если специфических кандидатов нет — берем следующие лучшие из пула
        if (explorationCandidates.empty()) {
            for (const auto& [movieId, _] : rankedList) {
                if (selectedIds.find(movieId) == selectedIds.end()) {
                    explorationCandidates.push_back(movieCache[movieId]);
                }
            }
        }

        for (const auto& cand : explorationCandidates) {
            if (result.size() >= count) {
                break;
            }
            if (selectedIds.insert(cand.id).second) {
                result.push_back(cand);
            }
        }
    }

    // Добираем оставшиеся, если exploration кандидатов не хватило
    for (const auto& [movieId, _] : rankedList) {
        if (result.size() >= count) {
            break;
        }
        if (selectedIds.insert(movieId).second) {
            result.push_back(movieCache[movieId]);
        }
    }

    return result;
}

} // namespace recommendation
