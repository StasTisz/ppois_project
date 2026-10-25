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
 *
 * Создает клавиатуры главного меню, мультивыбора жанров (с динамическими галочками),
 * кнопок реакций под карточкой фильма и шкалы оценивания от 1 до 10.
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
     *
     * Ряды:
     * 1: [🎬 Рекомендации (menu:recommend)] [🎭 Жанры (menu:genres)]
     * 2: [📜 История (menu:history)] [ℹ️ Помощь (menu:help)]
     *
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createMainMenuKeyboard();

    /**
     * @brief Создает клавиатуру мультивыбора любимых жанров с динамическими галочками.
     *
     * Генерирует сетку жанров по 2 кнопки в ряду. Для жанров, присутствующих
     * в `selectedGenres`, текст начинается с "✅ ", для остальных — без префикса.
     * Последний ряд содержит кнопку "✔️ Готово" (callback: genre:confirm).
     *
     * @param selectedGenres Список уже выбранных пользователем жанров.
     * @param availableGenres Доступные для выбора жанры (по умолчанию getDefaultGenres()).
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createGenreSelectionKeyboard(
        const std::vector<std::string>& selectedGenres,
        const std::vector<std::string>& availableGenres = getDefaultGenres());

    /**
     * @brief Создает клавиатуру реакций под карточкой фильма.
     *
     * Ряды:
     * 1: [👍 Понравился (rec:like)] [👎 Не понравился (rec:dislike)]
     * 2: [⏭ Следующий (rec:next)] [⭐ Оценить (1-10) (rec:rate)]
     * 3: [🏠 В меню (rec:menu)]
     *
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createMovieReactionKeyboard();

    /**
     * @brief Создает клавиатуру для выставления оценки фильму от 1 до 10.
     *
     * Ряды:
     * 1: [1] [2] [3] [4] [5] (rate:1 .. rate:5)
     * 2: [6] [7] [8] [9] [10] (rate:6 .. rate:10)
     * 3: [🔙 Отмена (rate:cancel)]
     *
     * @return Объект InlineKeyboard.
     */
    static InlineKeyboard createRatingKeyboard();
};

} // namespace bot
