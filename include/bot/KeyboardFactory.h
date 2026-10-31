#pragma once

#include "bot/InlineKeyboard.h"

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
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createMovieReactionKeyboard(int64_t movieId = 0, const std::string& trailerUrl = "");

    /**
     * @brief Создает клавиатуру для выставления оценки фильму от 1 до 10.
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createRatingKeyboard();

private:
    static InlineButton createGenreButton(
        const std::string& genre, const std::vector<std::string>& selectedGenres);
    static InlineKeyboardRow createRatingRow(int from, int to);
};

} // namespace bot
