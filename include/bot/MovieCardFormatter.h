#pragma once

#include "bot/IBotMessenger.h"
#include "bot/InlineKeyboard.h"
#include "core/Movie.h"

#include <cstddef>
#include <string>

/**
 * @file MovieCardFormatter.h
 * @brief Форматер карточки фильма для отправки в Telegram (текст, постеры, лимиты длины).
 */

namespace bot {

/**
 * @class MovieCardFormatter
 * @brief Форматирует информацию о фильме в структурированное сообщение для Telegram.
 *
 * Обеспечивает:
 * - Форматирование названия, года, рейтинга, жанров, режиссера и синопсиса;
 * - Валидацию URL постера (проверка схем http/https);
 * - Корректную обрезку описания без повреждения многобайтовых UTF-8 символов с соблюдением лимитов Telegram;
 * - Отправку фото с подписью и fallback на текстовое сообщение при сбое загрузки постера.
 */
class MovieCardFormatter {
public:
    /// Максимальная длина подписи к фото в Telegram Bot API (1024 байта).
    static constexpr size_t kMaxPhotoCaptionLength = 1024;

    /// Максимальная длина стандартного текстового сообщения в Telegram (4096 байт).
    static constexpr size_t kMaxTextMessageLength = 4096;

    /**
     * @brief Проверяет, содержит ли фильм валидный URL постера для отправки через Telegram API.
     * @param movie Объект фильма.
     * @return true, если ссылка не пуста и начинается с "http://" или "https://".
     */
    [[nodiscard]] static bool hasValidPosterUrl(const core::Movie& movie);

    /**
     * @brief Безопасно обрезает строку по границе UTF-8 символов с добавлением многоточия "...".
     * @param text Исходный текст.
     * @param maxBytes Максимально допустимый размер в байтах.
     * @return Обрезанная строка длиной не более maxBytes, не содержащая поврежденных символов.
     */
    [[nodiscard]] static std::string truncateUtf8Safe(const std::string& text, size_t maxBytes);

    /**
     * @brief Форматирует карточку фильма в текстовую подпись/сообщение с соблюдением лимита длины.
     * @param movie Объект фильма с метаданными.
     * @param maxLength Максимальная длина итогового текста (по умолчанию kMaxPhotoCaptionLength).
     * @param notice Опциональное вступительное уведомление о предыдущем действии пользователя.
     * @return Отформатированная строка.
     */
    [[nodiscard]] static std::string formatMovieCard(
        const core::Movie& movie,
        size_t maxLength = kMaxPhotoCaptionLength,
        const std::string& notice = "");

    /**
     * @brief Отправляет карточку фильма в чат Telegram: фото с подписью, либо текст с клавиатурой.
     * Если у фильма есть постер, пытается отправить через sendPhotoWithKeyboard.
     * При сбое загрузки постера или отсутствии URL отправляет текстовую карточку.
     * @param messenger Клиент отправки сообщений Telegram.
     * @param chatId Идентификатор чата.
     * @param movie Объект фильма.
     * @param keyboard Структура кнопок реакций.
     * @param notice Опциональное вступительное уведомление о предыдущем действии пользователя.
     */
    static void sendMovieCard(
        IBotMessenger& messenger,
        int64_t chatId,
        const core::Movie& movie,
        const InlineKeyboard& keyboard,
        const std::string& notice = "");
};

} // namespace bot
