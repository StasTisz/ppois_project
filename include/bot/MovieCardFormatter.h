#pragma once

#include "bot/BotConstants.h"
#include "bot/IBotMessenger.h"
#include "bot/InlineKeyboard.h"
#include "core/Movie.h"

#include <cstddef>
#include <functional>
#include <string>

/**
 * @file MovieCardFormatter.h
 * @brief Форматер карточки фильма для отправки в Telegram с поддержкой ленивого кэширования постеров.
 */

namespace bot {

/**
 * @class MovieCardFormatter
 * @brief Форматирует информацию о фильме в структурированное сообщение для Telegram.
 */
class MovieCardFormatter {
public:
    static constexpr size_t kMaxPhotoCaptionLength = kMaxPhotoCaptionBytes;
    static constexpr size_t kMaxTextMessageLength = kMaxTextMessageBytes;

    [[nodiscard]] static bool hasValidPosterUrl(const core::Movie& movie);

    [[nodiscard]] static std::string truncateUtf8Safe(const std::string& text, size_t maxBytes);

    /**
     * @brief Форматирует карточку фильма в текстовую подпись/сообщение с соблюдением лимита длины.
     */
    [[nodiscard]] static std::string formatMovieCard(
        const core::Movie& movie,
        size_t maxLength = kMaxPhotoCaptionLength,
        const std::string& notice = "",
        const std::string& explanation = "",
        const std::string& userStatus = "");

    /**
     * @brief Отправляет карточку фильма с поддержкой UI-подписей и ленивого кэширования poster_file_id.
     */
    static void sendMovieCard(
        IBotMessenger& messenger,
        int64_t chatId,
        core::Movie& movie,
        const InlineKeyboard& keyboard,
        const std::string& notice = "",
        const std::string& explanation = "",
        const std::string& userStatus = "",
        std::function<void(int64_t, const std::string&)> onFileIdCached = nullptr);

    /**
     * @brief Перегрузка для обратной совместимости тестов со storageChatId.
     */
    static void sendMovieCard(
        IBotMessenger& messenger,
        int64_t chatId,
        core::Movie& movie,
        const InlineKeyboard& keyboard,
        int64_t storageChatId,
        std::function<void(int64_t, const std::string&)> onFileIdCached = nullptr);

    /**
     * @brief Отправляет карточку фильма в чат Telegram для константного объекта фильма.
     */
    static void sendMovieCard(
        IBotMessenger& messenger,
        int64_t chatId,
        const core::Movie& movie,
        const InlineKeyboard& keyboard,
        const std::string& notice = "",
        const std::string& explanation = "",
        const std::string& userStatus = "");

private:
    static size_t getUtf8SequenceLength(unsigned char lead);
    static std::string formatCardHeader(const core::Movie& movie, const std::string& notice);
    static std::string formatCardBody(
        const core::Movie& movie, const std::string& explanation, const std::string& userStatus);
    static bool trySendCachedPoster(
        IBotMessenger& messenger, int64_t chatId, core::Movie& movie,
        const std::string& caption, const InlineKeyboard& keyboard);
    static bool trySendUrlPoster(
        IBotMessenger& messenger, int64_t chatId, core::Movie& movie,
        const std::string& caption, const InlineKeyboard& keyboard,
        const std::function<void(int64_t, const std::string&)>& onFileIdCached);
};

} // namespace bot
