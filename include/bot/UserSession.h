#pragma once

#include "core/Movie.h"
#include "bot/BotState.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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
 *
 * Предоставляет методы инкапсуляции мутаций согласно принципу "Tell, Don't Ask".
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

    /// Краткое уведомление о последнем действии пользователя (для следующей карточки).
    std::string lastActionNotice;

    // --- Методы инкапсуляции работы с жанрами ---

    [[nodiscard]] bool hasGenre(const std::string& genre) const {
        return std::find(selectedGenres.begin(), selectedGenres.end(), genre) != selectedGenres.end();
    }

    bool toggleGenre(const std::string& genre) {
        auto it = std::find(selectedGenres.begin(), selectedGenres.end(), genre);
        if (it != selectedGenres.end()) {
            selectedGenres.erase(it);
            return false;
        }
        selectedGenres.push_back(genre);
        return true;
    }

    [[nodiscard]] bool hasSelectedGenres() const {
        return !selectedGenres.empty();
    }

    [[nodiscard]] const std::vector<std::string>& getSelectedGenres() const {
        return selectedGenres;
    }

    void setSelectedGenres(std::vector<std::string> genres) {
        selectedGenres = std::move(genres);
    }

    void clearSelectedGenres() {
        selectedGenres.clear();
    }

    // --- Методы инкапсуляции работы с оценками ("Tell, Don't Ask") ---

    void recordRating(int64_t movieId, int score) {
        movieRatings[movieId] = score;
        skippedMovieIds.erase(movieId);
    }

    [[nodiscard]] bool hasRating(int64_t movieId) const {
        return movieRatings.find(movieId) != movieRatings.end();
    }

    [[nodiscard]] std::optional<int> getRating(int64_t movieId) const {
        auto it = movieRatings.find(movieId);
        if (it != movieRatings.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    [[nodiscard]] bool hasRatings() const {
        return !movieRatings.empty();
    }

    [[nodiscard]] size_t getRatingCount() const {
        return movieRatings.size();
    }

    [[nodiscard]] const std::unordered_map<int64_t, int>& getMovieRatings() const {
        return movieRatings;
    }

    void clearRatings() {
        movieRatings.clear();
    }

    // --- Методы инкапсуляции работы с текущим фильмом ---

    void setCurrentMovie(const std::optional<core::Movie>& movie) {
        currentMovie = movie;
    }

    void clearCurrentMovie() {
        currentMovie = std::nullopt;
    }

    [[nodiscard]] bool hasCurrentMovie() const {
        return currentMovie.has_value();
    }

    [[nodiscard]] const std::optional<core::Movie>& getCurrentMovie() const {
        return currentMovie;
    }

    // --- Методы инкапсуляции пропусков (Skipped) ---

    void markSkipped(int64_t movieId) {
        skippedMovieIds.insert(movieId);
    }

    void clearSkipped() {
        skippedMovieIds.clear();
    }

    [[nodiscard]] bool isSkipped(int64_t movieId) const {
        return skippedMovieIds.find(movieId) != skippedMovieIds.end();
    }

    // --- Методы уведомления о последнем действии ---

    void setLastActionNotice(std::string notice) {
        lastActionNotice = std::move(notice);
    }

    [[nodiscard]] const std::string& getLastActionNotice() const {
        return lastActionNotice;
    }

    [[nodiscard]] std::string consumeLastActionNotice() {
        std::string notice = std::move(lastActionNotice);
        lastActionNotice.clear();
        return notice;
    }

    void clearLastActionNotice() {
        lastActionNotice.clear();
    }

    // --- Полный сброс контекста сессии ---

    void resetSessionData() {
        selectedGenres.clear();
        currentMovie.reset();
        movieRatings.clear();
        skippedMovieIds.clear();
        lastActionNotice.clear();
    }
};

} // namespace bot
