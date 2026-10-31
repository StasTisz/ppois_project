#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @file BotConstants.h
 * @brief Централизованные именованные константы клиентского слоя Telegram-бота.
 */

namespace bot {

/// Рейтинг для быстрой положительной реакции (кнопка 👍 Like).
inline constexpr int kRatingLike = 10;

/// Рейтинг для быстрой отрицательной реакции (кнопка 👎 Dislike).
inline constexpr int kRatingDislike = 1;

/// Минимально допустимая оценка фильма пользователем.
inline constexpr int kMinRating = 1;

/// Максимально допустимая оценка фильма пользователем.
inline constexpr int kMaxRating = 10;

/// Порог положительной оценки для включения контентных рекомендаций.
inline constexpr int kPositiveRatingThreshold = 7;

/// Лимит количества похожих фильмов для отображения по умолчанию.
inline constexpr size_t kDefaultSimilarMoviesLimit = 3;

/// Размер батча кандидатов рекомендаций, запрашиваемых у RecommendationService.
inline constexpr size_t kRecommendationBatchSize = 20;

/// Таймаут подавления повторных кликов (debounce) в миллисекундах.
inline constexpr int kDebounceTimeoutMs = 300;

/// Размер пула рабочих потоков по умолчанию.
inline constexpr size_t kDefaultThreadPoolSize = 4;

/// Таймаут long-polling запросов к серверам Telegram по умолчанию (в секундах).
inline constexpr int kDefaultLongPollTimeoutSeconds = 10;

/// Лимит обновлений за один long-polling запрос к Telegram API.
inline constexpr int32_t kDefaultLongPollLimit = 100;

/// Количество кнопок жанров в одном ряду inline-клавиатуры.
inline constexpr size_t kGenresPerRow = 2;

/// Количество кнопок оценок в одном ряду шкалы 1-10.
inline constexpr size_t kRatingsPerRow = 5;

/// Максимальная длина подписи к фото в Telegram (в байтах).
inline constexpr size_t kMaxPhotoCaptionBytes = 1024;

/// Максимальная длина текстового сообщения в Telegram (в байтах).
inline constexpr size_t kMaxTextMessageBytes = 4096;

/// Максимальное количество попыток повторной отправки медиа при сетевых сбоях.
inline constexpr int kMediaMaxRetries = 1;

/// Задержка между повторными попытками отправки медиа (в миллисекундах).
inline constexpr int kMediaRetryDelayMs = 250;

} // namespace bot
