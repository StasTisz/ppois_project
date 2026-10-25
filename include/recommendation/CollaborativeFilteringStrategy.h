#pragma once

#include "recommendation/IRecommendationStrategy.h"
#include "repository/IMovieRepository.h"
#include "repository/IUserRepository.h"

#include <memory>
#include <string>
#include <vector>

namespace recommendation {

/**
 * @class CollaborativeFilteringStrategy
 * @brief Стратегия коллаборативной фильтрации (User-User Collaborative Filtering).
 *
 * Находит похожих пользователей на основе косинусного сходства их векторов оценок
 * и предсказывает рейтинг непросмотренных фильмов как взвешенную сумму оценок соседей.
 */
class CollaborativeFilteringStrategy : public IRecommendationStrategy {
public:
    CollaborativeFilteringStrategy(
        std::shared_ptr<repository::IMovieRepository> movieRepo,
        std::shared_ptr<repository::IUserRepository> userRepo,
        size_t minCommonRatings = 1
    );

    ~CollaborativeFilteringStrategy() override = default;

    [[nodiscard]] std::string getName() const override;

    /**
     * @brief Генерирует рекомендации на основе оценок похожих пользователей.
     * @param userId Идентификатор целевого пользователя.
     * @param count Желаемое количество фильмов.
     * @return Вектор рекомендуемых фильмов, отсортированных по убыванию прогнозного рейтинга.
     */
    std::vector<core::Movie> recommend(int64_t userId, size_t count) override;

    /**
     * @brief Вычисляет косинусное сходство между векторами оценок двух пользователей.
     * @param ratingsA Карта оценок первого пользователя (movieId -> score).
     * @param ratingsB Карта оценок второго пользователя (movieId -> score).
     * @return Сходство в диапазоне [-1.0, 1.0].
     */
    [[nodiscard]] double calculateCosineSimilarity(
        const std::unordered_map<int64_t, int>& ratingsA,
        const std::unordered_map<int64_t, int>& ratingsB
    ) const;

private:
    std::shared_ptr<repository::IMovieRepository> movieRepo_;
    std::shared_ptr<repository::IUserRepository> userRepo_;
    size_t minCommonRatings_{1};
};

} // namespace recommendation
