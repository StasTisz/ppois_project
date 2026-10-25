#pragma once

#include "recommendation/IRecommendationStrategy.h"
#include "repository/IMovieRepository.h"
#include "repository/IUserRepository.h"

#include <memory>
#include <string>
#include <vector>

namespace recommendation {

/**
 * @class HybridRecommendationStrategy
 * @brief Гибридная стратегия рекомендаций, объединяющая контентный, коллаборативный и популярный подходы.
 *
 * Динамически взвешивает компоненты в зависимости от глубины профиля пользователя (решая Cold Start),
 * а также реализует баланс Exploration vs Exploitation (исследование новых жанров vs эксплуатация известных предпочтений).
 */
class HybridRecommendationStrategy : public IRecommendationStrategy {
public:
    HybridRecommendationStrategy(
        std::shared_ptr<repository::IMovieRepository> movieRepo,
        std::shared_ptr<repository::IUserRepository> userRepo,
        std::shared_ptr<IRecommendationStrategy> popularityStrategy,
        std::shared_ptr<IRecommendationStrategy> contentStrategy,
        std::shared_ptr<IRecommendationStrategy> collabStrategy,
        double explorationRate = 0.15
    );

    ~HybridRecommendationStrategy() override = default;

    [[nodiscard]] std::string getName() const override;

    std::vector<core::Movie> recommend(int64_t userId, size_t count) override;

    void setExplorationRate(double rate);
    [[nodiscard]] double getExplorationRate() const;

private:
    std::shared_ptr<repository::IMovieRepository> movieRepo_;
    std::shared_ptr<repository::IUserRepository> userRepo_;
    std::shared_ptr<IRecommendationStrategy> popularityStrategy_;
    std::shared_ptr<IRecommendationStrategy> contentStrategy_;
    std::shared_ptr<IRecommendationStrategy> collabStrategy_;
    double explorationRate_{0.15}; // Эпсилон для баланса Exploration vs Exploitation
};

} // namespace recommendation
