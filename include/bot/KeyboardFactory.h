#pragma once

#include "bot/InlineKeyboard.h"
#include "core/Movie.h"

#include <string>
#include <vector>

/**
 * @file KeyboardFactory.h
 * @brief Фабрика компонентов интерфейса (Inline-клавиатур) Telegram-бота.
 */

namespace bot {

/**
 * @class KeyboardFactory
 * @brief Предоставляет статические методы для построения стандартных Inline-клавиатур.
 */
class KeyboardFactory {
public:
    /**
     * @brief Возвращает базовый список доступных жанров фильмов.
     * @return Константная ссылка на вектор названий жанров.
     */
    static const std::vector<std::string>& getDefaultGenres();

    /**
     * @brief Создает клавиатуру главного меню.
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createMainMenuKeyboard();

    /**
     * @brief Создает клавиатуру мультивыбора любимых жанров с динамическими галочками.
     * @param selectedGenres Список уже выбранных пользователем жанров.
     * @param availableGenres Доступные для выбора жанры (по умолчанию getDefaultGenres()).
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createGenreSelectionKeyboard(
        const std::vector<std::string>& selectedGenres,
        const std::vector<std::string>& availableGenres = getDefaultGenres());

    /**
     * @brief Создает клавиатуру реакций под карточкой фильма.
     * @param movieId Идентификатор фильма для защиты от кликов по старым карточкам.
     * @param trailerUrl Ссылка на YouTube-трейлер фильма (опционально).
     * @param isBookmarked Флаг нахождения фильма в закладках пользователя.
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createMovieReactionKeyboard(
        int64_t movieId = 0, const std::string& trailerUrl = "", bool isBookmarked = false);

    /**
     * @brief Создает клавиатуру для выставления оценки фильму от 1 до 10.
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createRatingKeyboard();

    /**
     * @brief Создает клавиатуру с кнопками выбора найденных поиском фильмов.
     * @param movies Список найденных фильмов.
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createSearchResultsKeyboard(const std::vector<core::Movie>& movies);

    /**
     * @brief Создает клавиатуру истории оценок с кнопками изменения оценок для каждого фильма.
     * @param items Список пар (movieId, текстовая подпись кнопки).
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createHistoryKeyboard(const std::vector<std::pair<int64_t, std::string>>& items);

private:
    static InlineButton createGenreButton(
        const std::string& genre, const std::vector<std::string>& selectedGenres);
    static InlineKeyboardRow createRatingRow(int from, int to);
};

} // namespace bot
