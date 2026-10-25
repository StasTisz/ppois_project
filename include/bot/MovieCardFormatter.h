#pragma once

#include "bot/IBotMessenger.h"
#include "bot/InlineKeyboard.h"
#include "core/Movie.h"

#include <cstddef>
#include <string>
#include <functional>

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
    static constexpr size_t kMaxPhotoCaptionLength = 1024;
    static constexpr size_t kMaxTextMessageLength = 4096;

    [[nodiscard]] static bool hasValidPosterUrl(const core::Movie& movie);

    [[nodiscard]] static std::string truncateUtf8Safe(const std::string& text, size_t maxBytes);

    [[nodiscard]] static std::string formatMovieCard(
        const core::Movie& movie,
        size_t maxLength = kMaxPhotoCaptionLength);

    /**
     * @brief Отправляет карточку фильма с прямой доставкой пользователю и кэшированием poster_file_id.
     * @param messenger Клиент отправки сообщений.
     * @param chatId Идентификатор целевого чата пользователя.
     * @param movie Объект фильма (кэширует file_id в movie.posterFileId).
     * @param keyboard Клавиатура реакций.
     * @param storageChatId Не используется (сохранено для обратной совместимости сигнатуры).
     * @param onFileIdCached Callback для сохранения полученного от Telegram file_id в базу данных.
     */
    static void sendMovieCard(
        IBotMessenger& messenger,
        int64_t chatId,
        core::Movie& movie,
        const InlineKeyboard& keyboard,
        int64_t storageChatId = 0,
        std::function<void(int64_t, const std::string&)> onFileIdCached = nullptr);

    /**
     * @brief Перегрузка для константного объекта фильма (обратная совместимость).
     */
    static void sendMovieCard(
        IBotMessenger& messenger,
        int64_t chatId,
        const core::Movie& movie,
        const InlineKeyboard& keyboard) {
        core::Movie copy = movie;
        sendMovieCard(messenger, chatId, copy, keyboard, 0, nullptr);
    }
};

} // namespace bot
