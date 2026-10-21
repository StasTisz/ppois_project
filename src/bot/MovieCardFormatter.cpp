#include "bot/MovieCardFormatter.h"

#include <iomanip>
#include <sstream>

/**
 * @file MovieCardFormatter.cpp
 * @brief Реализация методов форматирования карточки фильма и отправки медиа-сообщений.
 */

namespace bot {

bool MovieCardFormatter::hasValidPosterUrl(const core::Movie& movie) {
    if (movie.posterUrl.empty()) {
        return false;
    }
    return (movie.posterUrl.rfind("https://", 0) == 0 ||
            movie.posterUrl.rfind("http://", 0) == 0);
}

std::string MovieCardFormatter::truncateUtf8Safe(const std::string& text, size_t maxBytes) {
    if (text.size() <= maxBytes) {
        return text;
    }

    if (maxBytes <= 3) {
        return std::string("...", maxBytes);
    }

    size_t targetLimit = maxBytes - 3;
    if (targetLimit >= text.size()) {
        return text;
    }

    size_t idx = targetLimit;
    // Если попали на продолжение многобайтового символа (10xxxxxx), отматываем к началу
    while (idx > 0 && (static_cast<unsigned char>(text[idx]) & 0xC0) == 0x80) {
        --idx;
    }

    // Проверяем, помещается ли начальный символ целиком в targetLimit
    if (idx < targetLimit) {
        unsigned char lead = static_cast<unsigned char>(text[idx]);
        size_t charLen = 1;
        if ((lead & 0xE0) == 0xC0) {
            charLen = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            charLen = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            charLen = 4;
        }

        if (idx + charLen <= targetLimit) {
            idx += charLen;
        }
    }

    return text.substr(0, idx) + "...";
}

std::string MovieCardFormatter::formatMovieCard(const core::Movie& movie, size_t maxLength) {
    std::ostringstream prefix;

    prefix << "🎬 " << movie.title;
    if (movie.year > 0) {
        prefix << " (" << movie.year << ")";
    }
    prefix << "\n";

    prefix << "⭐ Рейтинг: " << std::fixed << std::setprecision(1) << movie.ratingAvg << " / 10\n";

    prefix << "📁 Жанры: ";
    if (movie.genres.empty()) {
        prefix << "Не указаны\n";
    } else {
        for (size_t i = 0; i < movie.genres.size(); ++i) {
            prefix << movie.genres[i];
            if (i + 1 < movie.genres.size()) {
                prefix << ", ";
            }
        }
        prefix << "\n";
    }

    if (!movie.director.empty()) {
        prefix << "👤 Режиссер: " << movie.director << "\n";
    }

    const std::string suffix = "\nВыберите реакцию кнопками ниже:";
    const std::string descHeader = "\n📝 Описание:\n";

    std::string prefixStr = prefix.str();

    if (!movie.description.empty()) {
        size_t fixedSize = prefixStr.size() + descHeader.size() + suffix.size();
        if (fixedSize + movie.description.size() <= maxLength) {
            return prefixStr + descHeader + movie.description + suffix;
        }

        if (fixedSize < maxLength) {
            size_t availableDesc = maxLength - fixedSize;
            std::string truncatedDesc = truncateUtf8Safe(movie.description, availableDesc);
            return prefixStr + descHeader + truncatedDesc + suffix;
        }

        return truncateUtf8Safe(prefixStr + suffix, maxLength);
    }

    if (prefixStr.size() + suffix.size() <= maxLength) {
        return prefixStr + suffix;
    }

    return truncateUtf8Safe(prefixStr + suffix, maxLength);
}

void MovieCardFormatter::sendMovieCard(
    IBotMessenger& messenger,
    int64_t chatId,
    const core::Movie& movie,
    const InlineKeyboard& keyboard) {
    std::string caption = formatMovieCard(movie, kMaxPhotoCaptionLength);

    if (hasValidPosterUrl(movie)) {
        bool sent = messenger.sendPhotoWithKeyboard(chatId, movie.posterUrl, caption, keyboard);
        if (!sent) {
            messenger.sendMessageWithKeyboard(chatId, caption, keyboard);
        }
    } else {
        messenger.sendMessageWithKeyboard(chatId, caption, keyboard);
    }
}

} // namespace bot
