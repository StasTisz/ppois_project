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

size_t MovieCardFormatter::getUtf8SequenceLength(unsigned char lead) {
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

std::string MovieCardFormatter::truncateUtf8Safe(const std::string& text, size_t maxBytes) {
    if (text.size() <= maxBytes) return text;
    if (maxBytes <= 3) return std::string("...", maxBytes);

    size_t targetLimit = maxBytes - 3;
    if (targetLimit >= text.size()) return text;

    size_t idx = targetLimit;
    while (idx > 0 && (static_cast<unsigned char>(text[idx]) & 0xC0) == 0x80) {
        --idx;
    }

    if (idx < targetLimit) {
        size_t charLen = getUtf8SequenceLength(static_cast<unsigned char>(text[idx]));
        if (idx + charLen <= targetLimit) {
            idx += charLen;
        }
    }
    return text.substr(0, idx) + "...";
}

std::string MovieCardFormatter::formatCardHeader(const core::Movie& movie, const std::string& notice) {
    std::ostringstream oss;
    if (!notice.empty()) {
        oss << notice << "\n\n";
    }
    oss << "🎬 " << movie.title;
    if (movie.year > 0) {
        oss << " (" << movie.year << ")";
    }
    oss << "\n⭐ Рейтинг: " << std::fixed << std::setprecision(1) << movie.ratingAvg << " / 10\n";
    return oss.str();
}

static std::string formatGenresList(const std::vector<std::string>& genres) {
    if (genres.empty()) return "Не указаны\n";
    std::string res;
    for (size_t i = 0; i < genres.size(); ++i) {
        res += genres[i] + (i + 1 < genres.size() ? ", " : "\n");
    }
    return res;
}

std::string MovieCardFormatter::formatCardBody(
    const core::Movie& movie, const std::string& explanation, const std::string& userStatus) {
    std::ostringstream oss;
    oss << "📁 Жанры: " << formatGenresList(movie.genres);
    if (!movie.director.empty()) oss << "👤 Режиссер: " << movie.director << "\n";
    if (!explanation.empty()) oss << "\n" << explanation << "\n";
    if (!userStatus.empty()) oss << userStatus << "\n";
    return oss.str();
}

std::string MovieCardFormatter::formatMovieCard(
    const core::Movie& movie, size_t maxLength,
    const std::string& notice, const std::string& explanation, const std::string& userStatus) {
    std::string prefixStr = formatCardHeader(movie, notice) + formatCardBody(movie, explanation, userStatus);
    const std::string suffix = "\nВыберите реакцию кнопками ниже:";
    const std::string descHeader = "\n📝 Описание:\n";

    if (!movie.description.empty()) {
        size_t fixedSize = prefixStr.size() + descHeader.size() + suffix.size();
        if (fixedSize + movie.description.size() <= maxLength) {
            return prefixStr + descHeader + movie.description + suffix;
        }
        if (fixedSize < maxLength) {
            return prefixStr + descHeader + truncateUtf8Safe(movie.description, maxLength - fixedSize) + suffix;
        }
    }
    return truncateUtf8Safe(prefixStr + suffix, maxLength);
}

bool MovieCardFormatter::trySendCachedPoster(
    IBotMessenger& messenger, int64_t chatId, core::Movie& movie,
    const std::string& caption, const InlineKeyboard& keyboard) {
    if (movie.posterFileId.empty()) {
        return false;
    }
    if (messenger.sendPhotoWithKeyboard(chatId, movie.posterFileId, caption, keyboard, nullptr)) {
        return true;
    }
    movie.posterFileId.clear();
    return false;
}

bool MovieCardFormatter::trySendUrlPoster(
    IBotMessenger& messenger, int64_t chatId, core::Movie& movie,
    const std::string& caption, const InlineKeyboard& keyboard,
    const std::function<void(int64_t, const std::string&)>& onFileIdCached) {
    if (!hasValidPosterUrl(movie)) {
        return false;
    }
    std::string obtainedId;
    if (messenger.sendPhotoWithKeyboard(chatId, movie.posterUrl, caption, keyboard, &obtainedId)) {
        if (!obtainedId.empty()) {
            movie.posterFileId = obtainedId;
            if (onFileIdCached) onFileIdCached(movie.id, obtainedId);
        }
        return true;
    }
    return false;
}

void MovieCardFormatter::sendMovieCard(
    IBotMessenger& messenger, int64_t chatId, core::Movie& movie,
    const InlineKeyboard& keyboard, const std::string& notice,
    const std::string& explanation, const std::string& userStatus,
    std::function<void(int64_t, const std::string&)> onFileIdCached) {
    std::string caption = formatMovieCard(movie, kMaxPhotoCaptionLength, notice, explanation, userStatus);
    if (trySendCachedPoster(messenger, chatId, movie, caption, keyboard)) {
        return;
    }
    if (trySendUrlPoster(messenger, chatId, movie, caption, keyboard, onFileIdCached)) {
        return;
    }
    messenger.sendMessageWithKeyboard(chatId, caption, keyboard);
}

void MovieCardFormatter::sendMovieCard(
    IBotMessenger& messenger, int64_t chatId, core::Movie& movie,
    const InlineKeyboard& keyboard, int64_t /* storageChatId */,
    std::function<void(int64_t, const std::string&)> onFileIdCached) {
    sendMovieCard(messenger, chatId, movie, keyboard, "", "", "", std::move(onFileIdCached));
}

void MovieCardFormatter::sendMovieCard(
    IBotMessenger& messenger, int64_t chatId, const core::Movie& movie,
    const InlineKeyboard& keyboard, const std::string& notice,
    const std::string& explanation, const std::string& userStatus) {
    core::Movie mutableMovie = movie;
    sendMovieCard(messenger, chatId, mutableMovie, keyboard, notice, explanation, userStatus, nullptr);
}

} // namespace bot
