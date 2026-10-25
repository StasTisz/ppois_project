#include "analytics/MetricsCollector.h"

#include <iomanip>
#include <sstream>

namespace analytics {

void MetricsCollector::recordImpression(int64_t userId, int64_t movieId) {
    (void)movieId;
    std::lock_guard<std::mutex> lock(mutex_);
    ++totalImpressions_;
    userLastActivity_[userId] = std::chrono::system_clock::now();
}

void MetricsCollector::recordLike(int64_t userId, int64_t movieId) {
    (void)movieId;
    std::lock_guard<std::mutex> lock(mutex_);
    ++totalLikes_;
    userLastActivity_[userId] = std::chrono::system_clock::now();
}

void MetricsCollector::recordDislike(int64_t userId, int64_t movieId) {
    (void)movieId;
    std::lock_guard<std::mutex> lock(mutex_);
    ++totalDislikes_;
    userLastActivity_[userId] = std::chrono::system_clock::now();
}

void MetricsCollector::recordUserActivity(int64_t userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    userLastActivity_[userId] = std::chrono::system_clock::now();
}

void MetricsCollector::recordRecommendationLatency(std::chrono::microseconds duration) {
    std::lock_guard<std::mutex> lock(mutex_);
    totalLatencyMicroseconds_ += static_cast<uint64_t>(duration.count());
    ++totalRecommendationRequests_;
}

double MetricsCollector::getCTR() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (totalImpressions_ == 0) {
        return 0.0;
    }
    return static_cast<double>(totalLikes_) / static_cast<double>(totalImpressions_);
}

size_t MetricsCollector::getDailyActiveUsers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    auto dayAgo = now - std::chrono::hours(24);

    size_t activeCount = 0;
    for (const auto& [userId, lastTime] : userLastActivity_) {
        (void)userId;
        if (lastTime >= dayAgo) {
            ++activeCount;
        }
    }
    return activeCount;
}

double MetricsCollector::getAverageLatencyMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (totalRecommendationRequests_ == 0) {
        return 0.0;
    }
    double totalMs = static_cast<double>(totalLatencyMicroseconds_) / 1000.0;
    return totalMs / static_cast<double>(totalRecommendationRequests_);
}

size_t MetricsCollector::getTotalImpressions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalImpressions_;
}

size_t MetricsCollector::getTotalLikes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalLikes_;
}

size_t MetricsCollector::getTotalDislikes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalDislikes_;
}

MetricsSummary MetricsCollector::getSummary() const {
    std::lock_guard<std::mutex> lock(mutex_);
    MetricsSummary s;
    s.totalImpressions = totalImpressions_;
    s.totalLikes = totalLikes_;
    s.totalDislikes = totalDislikes_;
    s.ctr = (totalImpressions_ > 0) ? (static_cast<double>(totalLikes_) / static_cast<double>(totalImpressions_)) : 0.0;

    auto now = std::chrono::system_clock::now();
    auto dayAgo = now - std::chrono::hours(24);
    for (const auto& [_, lastTime] : userLastActivity_) {
        if (lastTime >= dayAgo) {
            ++s.dailyActiveUsers;
        }
    }

    s.totalRecommendationRequests = totalRecommendationRequests_;
    s.averageLatencyMs = (totalRecommendationRequests_ > 0)
        ? ((static_cast<double>(totalLatencyMicroseconds_) / 1000.0) / static_cast<double>(totalRecommendationRequests_))
        : 0.0;
    return s;
}

std::string MetricsCollector::formatReport() const {
    auto s = getSummary();
    std::ostringstream oss;
    oss << "📊 Метрики рекомендательной системы:\n"
        << "• Всего показов (Impressions): " << s.totalImpressions << "\n"
        << "• Лайков / Дизлайков: 👍 " << s.totalLikes << " / 👎 " << s.totalDislikes << "\n"
        << "• Конверсия (CTR): " << std::fixed << std::setprecision(2) << (s.ctr * 100.0) << "%\n"
        << "• Активных сессий за сутки (DAU): " << s.dailyActiveUsers << "\n"
        << "• Запросов рекомендаций: " << s.totalRecommendationRequests << "\n"
        << "• Среднее время подбора: " << std::fixed << std::setprecision(3) << s.averageLatencyMs << " мс";
    return oss.str();
}

void MetricsCollector::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    totalImpressions_ = 0;
    totalLikes_ = 0;
    totalDislikes_ = 0;
    userLastActivity_.clear();
    totalLatencyMicroseconds_ = 0;
    totalRecommendationRequests_ = 0;
}

} // namespace analytics
