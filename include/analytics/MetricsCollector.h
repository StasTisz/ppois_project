#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace analytics {

/**
 * @brief Сводная структура базовых продуктовых метрик рекомендательной системы.
 */
struct MetricsSummary {
    size_t totalImpressions{0};
    size_t totalLikes{0};
    size_t totalDislikes{0};
    double ctr{0.0};                     // Click-Through Rate: totalLikes / totalImpressions
    size_t dailyActiveUsers{0};           // Количество активных пользователей за последние 24 часа (DAU)
    double averageLatencyMs{0.0};         // Среднее время подбора рекомендаций (мс)
    size_t totalRecommendationRequests{0};
};

/**
 * @class MetricsCollector
 * @brief Потокобезопасный коллектор продуктовых метрик (CTR, DAU, Latency).
 */
class MetricsCollector {
public:
    MetricsCollector() = default;
    ~MetricsCollector() = default;

    /**
     * @brief Регистрирует показ карточки фильма пользователю.
     * @param userId Идентификатор пользователя.
     * @param movieId Идентификатор показанного фильма.
     */
    void recordImpression(int64_t userId, int64_t movieId);

    /**
     * @brief Регистрирует положительную реакцию (лайк / высокую оценку).
     * @param userId Идентификатор пользователя.
     * @param movieId Идентификатор фильма.
     */
    void recordLike(int64_t userId, int64_t movieId);

    /**
     * @brief Регистрирует отрицательную реакцию (дизлайк / низкую оценку).
     * @param userId Идентификатор пользователя.
     * @param movieId Идентификатор фильма.
     */
    void recordDislike(int64_t userId, int64_t movieId);

    /**
     * @brief Регистрирует активность пользователя для подсчета DAU.
     * @param userId Идентификатор пользователя.
     */
    void recordUserActivity(int64_t userId);

    /**
     * @brief Фиксирует длительность алгоритма подбора рекомендаций.
     * @param duration Длительность в микросекундах.
     */
    void recordRecommendationLatency(std::chrono::microseconds duration);

    /**
     * @brief Возвращает текущий CTR (отношение лайков к общему числу показов).
     * @return CTR в диапазоне [0.0, 1.0].
     */
    [[nodiscard]] double getCTR() const;

    /**
     * @brief Возвращает количество уникальных активных пользователей за последние 24 часа.
     * @return Число активных сессий/пользователей.
     */
    [[nodiscard]] size_t getDailyActiveUsers() const;

    /**
     * @brief Возвращает среднее время генерации рекомендаций в миллисекундах.
     * @return Время в мс.
     */
    [[nodiscard]] double getAverageLatencyMs() const;

    [[nodiscard]] size_t getTotalImpressions() const;
    [[nodiscard]] size_t getTotalLikes() const;
    [[nodiscard]] size_t getTotalDislikes() const;

    /**
     * @brief Возвращает снимок всех метрик в виде единой структуры.
     */
    [[nodiscard]] MetricsSummary getSummary() const;

    /**
     * @brief Форматирует текстовый отчет метрик для вывода в Telegram или консоль.
     */
    [[nodiscard]] std::string formatReport() const;

    /**
     * @brief Сбрасывает накопленные метрики (для тестов).
     */
    void reset();

private:
    mutable std::mutex mutex_;
    size_t totalImpressions_{0};
    size_t totalLikes_{0};
    size_t totalDislikes_{0};

    std::unordered_map<int64_t, std::chrono::system_clock::time_point> userLastActivity_;

    uint64_t totalLatencyMicroseconds_{0};
    size_t totalRecommendationRequests_{0};
};

} // namespace analytics
