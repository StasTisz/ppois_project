#include "bot/NetworkErrorHandler.h"

#include <algorithm>
#include <cctype>
#include <iostream>

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

} // namespace

NetworkErrorType NetworkErrorHandler::classifyError(const std::string& errorMessage) {
    if (errorMessage.empty()) {
        return NetworkErrorType::None;
    }

    std::string lower = toLower(errorMessage);

    // Ошибки медиа и ссылок на постеры
    if (lower.find("failed to get http url content") != std::string::npos ||
        lower.find("wrong file identifier") != std::string::npos ||
        lower.find("wrong url") != std::string::npos ||
        lower.find("invalid url") != std::string::npos ||
        lower.find("http url specified") != std::string::npos) {
        return NetworkErrorType::InvalidMediaUrl;
    }

    // Таймауты сети
    if (lower.find("timeout") != std::string::npos ||
        lower.find("timed out") != std::string::npos) {
        return NetworkErrorType::Timeout;
    }

    // Ограничение частоты запросов
    if (lower.find("too many requests") != std::string::npos ||
        lower.find("retry after") != std::string::npos ||
        lower.find("429") != std::string::npos) {
        return NetworkErrorType::RateLimit;
    }

    // Ошибки API
    if (lower.find("bad request") != std::string::npos ||
        lower.find("unauthorized") != std::string::npos ||
        lower.find("api error") != std::string::npos ||
        lower.find("telegram api") != std::string::npos) {
        return NetworkErrorType::ApiError;
    }

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
            std::string err = e.what();
            if (lastErrorOut) {
                *lastErrorOut = err;
            }

            auto type = classifyError(err);
            std::cerr << "[NetworkErrorHandler] Ошибка сети (попытка "
                      << (attempt + 1) << " из " << (config.maxRetries + 1) << "): "
                      << err << "\n";

            if (!isRetryable(type) || attempt == config.maxRetries) {
                return false;
            }

            if (config.retryDelayMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(config.retryDelayMs));
            }
        } catch (...) {
            if (lastErrorOut) {
                *lastErrorOut = "Неизвестное исключение non-std::exception";
            }
            std::cerr << "[NetworkErrorHandler] Неизвестное исключение при выполнении запроса\n";
            return false;
        }
    }

    return false;
}

} // namespace bot
