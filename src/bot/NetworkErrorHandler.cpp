#include "bot/NetworkErrorHandler.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string_view>

/**
 * @file NetworkErrorHandler.cpp
 * @brief Реализация классификатора сетевых ошибок и механизма повторных попыток.
 */

namespace bot {

namespace {

std::string toLower(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char ch : str) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return result;
}

bool isMediaError(std::string_view lower) {
    return lower.find("failed to get http url content") != std::string_view::npos ||
           lower.find("wrong file identifier") != std::string_view::npos ||
           lower.find("wrong url") != std::string_view::npos ||
           lower.find("invalid url") != std::string_view::npos ||
           lower.find("http url specified") != std::string_view::npos;
}

bool isTimeoutError(std::string_view lower) {
    return lower.find("timeout") != std::string_view::npos ||
           lower.find("timed out") != std::string_view::npos;
}

bool isRateLimitError(std::string_view lower) {
    return lower.find("too many requests") != std::string_view::npos ||
           lower.find("retry after") != std::string_view::npos ||
           lower.find("429") != std::string_view::npos;
}

bool isApiError(std::string_view lower) {
    return lower.find("bad request") != std::string_view::npos ||
           lower.find("unauthorized") != std::string_view::npos ||
           lower.find("api error") != std::string_view::npos ||
           lower.find("telegram api") != std::string_view::npos;
}

bool shouldRetryException(
    const std::exception& e, int attempt, const RetryConfig& config, std::string* lastErrorOut) {
    std::string err = e.what();
    if (lastErrorOut) *lastErrorOut = err;
    auto type = NetworkErrorHandler::classifyError(err);
    std::cerr << "[NetworkErrorHandler] Ошибка сети (попытка "
              << (attempt + 1) << " из " << (config.maxRetries + 1) << "): " << err << "\n";
    if (!NetworkErrorHandler::isRetryable(type) || attempt == config.maxRetries) return false;
    if (config.retryDelayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config.retryDelayMs));
    }
    return true;
}

} // namespace

NetworkErrorType NetworkErrorHandler::classifyError(const std::string& errorMessage) {
    if (errorMessage.empty()) return NetworkErrorType::None;

    std::string lower = toLower(errorMessage);
    if (isMediaError(lower)) return NetworkErrorType::InvalidMediaUrl;
    if (isTimeoutError(lower)) return NetworkErrorType::Timeout;
    if (isRateLimitError(lower)) return NetworkErrorType::RateLimit;
    if (isApiError(lower)) return NetworkErrorType::ApiError;

    return NetworkErrorType::Unknown;
}

bool NetworkErrorHandler::isRetryable(NetworkErrorType type) {
    switch (type) {
        case NetworkErrorType::Timeout:
        case NetworkErrorType::RateLimit:
        case NetworkErrorType::Unknown:
            return true;
        case NetworkErrorType::InvalidMediaUrl:
        case NetworkErrorType::ApiError:
        case NetworkErrorType::None:
        default:
            return false;
    }
}

bool NetworkErrorHandler::executeWithRetry(
    const std::function<void()>& operation,
    const RetryConfig& config,
    std::string* lastErrorOut) {
    for (int attempt = 0; attempt <= config.maxRetries; ++attempt) {
        try {
            operation();
            return true;
        } catch (const std::exception& e) {
            if (!shouldRetryException(e, attempt, config, lastErrorOut)) return false;
        } catch (...) {
            if (lastErrorOut) *lastErrorOut = "Неизвестное исключение non-std::exception";
            std::cerr << "[NetworkErrorHandler] Неизвестное исключение при выполнении запроса\n";
            return false;
        }
    }
    return false;
}

} // namespace bot
