#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <thread>

/**
 * @file NetworkErrorHandler.h
 * @brief Классификатор сетевых ошибок Telegram API и диспетчер повторных попыток (Retry Policy).
 */

namespace bot {

/**
 * @enum NetworkErrorType
 * @brief Категории сетевых ошибок и ошибок Telegram Bot API.
 */
enum class NetworkErrorType {
    None,            ///< Ошибок не обнаружено.
    Timeout,         ///< Таймаут сетевого соединения или чтения сокета.
    RateLimit,       ///< Превышение частоты запросов Telegram API (HTTP 429 Too Many Requests).
    InvalidMediaUrl, ///< Ошибка загрузки медиа по указанному URL (битый URL, 404, неверный формат).
    ApiError,        ///< Прочие ошибки Telegram Bot API (HTTP 400 Bad Request и др.).
    Unknown          ///< Неизвестное сетевое исключение.
};

/**
 * @struct RetryConfig
 * @brief Параметры политики повторных попыток при сбоях сети.
 */
struct RetryConfig {
    int maxRetries{2};   ///< Максимальное количество повторных попыток при сбое.
    int retryDelayMs{0}; ///< Задержка между повторными попытками в миллисекундах.
};

/**
 * @class NetworkErrorHandler
 * @brief Утилитарный класс для классификации сетевых ошибок и выполнения операций с повторными попытками.
 */
class NetworkErrorHandler {
public:
    /**
     * @brief Классифицирует тип ошибки на основе текста сообщения исключения.
     * @param errorMessage Текст сообщения об ошибке или e.what().
     * @return Категория NetworkErrorType.
     */
    [[nodiscard]] static NetworkErrorType classifyError(const std::string& errorMessage);

    /**
     * @brief Определяет, подлежит ли ошибка повторной попытке (временный сбой).
     * @param type Категория ошибки.
     * @return true, если операцию имеет смысл повторить, иначе false.
     */
    [[nodiscard]] static bool isRetryable(NetworkErrorType type);

    /**
     * @brief Выполняет переданную функцию с защитой try-catch и политикой повторных попыток.
     * @param operation Выполняемая операция (лямбда/функтор без аргументов).
     * @param config Параметры повторных попыток.
     * @param lastErrorOut Опциональный указатель на строку для сохранения текста последней ошибки.
     * @return true, если операция завершилась успешно; false, если все попытки исчерпаны.
     */
    static bool executeWithRetry(
        const std::function<void()>& operation,
        const RetryConfig& config = {},
        std::string* lastErrorOut = nullptr);
};

} // namespace bot
