#pragma once

#include "core/Movie.h"
#include "bot/BotState.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @file UserSession.h
 * @brief Структура данных сессии пользователя и состояния диалога.
 */

namespace bot {

/**
 * @struct UserSession
 * @brief Хранилище контекста диалога и пользовательских данных текущей сессии.
 *
 * Содержит идентификаторы пользователя и чата, текущее состояние FSM,
 * выбранные жанры, текущий фильм, историю выставленных в рамках сессии оценок
 * и множество пропущенных фильмов.
 */
struct UserSession {
    /// Уникальный идентификатор пользователя Telegram.
    int64_t userId{0};

    /// Идентификатор чата Telegram.
    int64_t chatId{0};

    /// Никнейм пользователя (username).
    std::string username;

    /// Текущее состояние диалогового автомата пользователя.
    BotState state{BotState::Idle};

    /// Список выбранных пользователем любимых жанров.
    std::vector<std::string> selectedGenres;

    /// Фильм, который пользователь просматривает или оценивает в данный момент.
    std::optional<core::Movie> currentMovie;

    /// Словарь выставленных в сессии оценок: ID фильма -> оценка (1-10).
    std::unordered_map<int64_t, int> movieRatings;

    /// Множество ID фильмов, пропущенных кнопкой «Следующий» без оценки.
    std::unordered_set<int64_t> skippedMovieIds;

    /**
     * @brief Проверяет, выбран ли указанный жанр в текущей сессии.
     * @param genre Название жанра.
     * @return true, если жанр присутствует в selectedGenres, иначе false.
     */
    [[nodiscard]] bool hasGenre(const std::string& genre) const {
        return std::find(selectedGenres.begin(), selectedGenres.end(), genre) != selectedGenres.end();
    }

    /**
     * @brief Переключает выбор жанра (добавляет, если отсутствует, либо удаляет, если уже выбран).
     * @param genre Название переключаемого жанра.
     * @return true, если жанр был добавлен, false, если жанр был удален.
     */
    bool toggleGenre(const std::string& genre) {
        auto it = std::find(selectedGenres.begin(), selectedGenres.end(), genre);
        if (it != selectedGenres.end()) {
            selectedGenres.erase(it);
            return false;
        }
        selectedGenres.push_back(genre);
        return true;
    }

    /**
     * @brief Отмечает фильм как пропущенный пользователем.
     * @param movieId Идентификатор пропущенного фильма.
     */
    void markSkipped(int64_t movieId) {
        skippedMovieIds.insert(movieId);
    }

    /**
     * @brief Проверяет, был ли фильм пропущен пользователем.
     * @param movieId Идентификатор фильма.
     * @return true, если фильм находится в списке пропущенных.
     */
    [[nodiscard]] bool isSkipped(int64_t movieId) const {
        return skippedMovieIds.find(movieId) != skippedMovieIds.end();
    }
};

} // namespace bot
