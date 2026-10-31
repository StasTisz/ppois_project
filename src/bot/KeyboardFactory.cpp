#include "bot/KeyboardFactory.h"

/**
 * @file KeyboardFactory.cpp
 * @brief Реализация методов фабрики интерфейса KeyboardFactory.
 */

namespace bot {

const std::vector<std::string>& KeyboardFactory::getDefaultGenres() {
    static const std::vector<std::string> genres = {
        "Sci-Fi", "Action", "Drama", "Comedy",
        "Thriller", "Animation", "Fantasy", "Horror"
    };
    return genres;
}

InlineKeyboard KeyboardFactory::createMainMenuKeyboard() {
    InlineKeyboard kb;
    kb.rows = {
        {
            {"🎬 Рекомендации", "menu:recommend"},
            {"🎭 Жанры", "menu:genres"}
        },
        {
            {"📜 История", "menu:history"},
            {"ℹ️ Помощь", "menu:help"}
        }
    };
    return kb;
}

InlineKeyboard KeyboardFactory::createGenreSelectionKeyboard(
    const std::vector<std::string>& selectedGenres,
    const std::vector<std::string>& availableGenres) {
    InlineKeyboard kb;
    InlineKeyboardRow currentRow;

    for (const auto& genre : availableGenres) {
        bool isSelected = false;
        for (const auto& sel : selectedGenres) {
            if (sel == genre) {
                isSelected = true;
                break;
            }
        }

        std::string text = isSelected ? ("✅ " + genre) : genre;
        currentRow.push_back({std::move(text), "genre:toggle:" + genre});

        if (currentRow.size() == 2) {
            kb.rows.push_back(std::move(currentRow));
            currentRow.clear();
        }
    }

    if (!currentRow.empty()) {
        kb.rows.push_back(std::move(currentRow));
    }

    kb.rows.push_back({{"✔️ Готово", "genre:confirm"}});
    return kb;
}

InlineKeyboard KeyboardFactory::createMovieReactionKeyboard(int64_t movieId) {
    std::string suffix = (movieId > 0) ? (":" + std::to_string(movieId)) : "";

    InlineKeyboard kb;
    kb.rows = {
        {
            {"👍 Понравился", "rec:like" + suffix},
            {"👎 Не понравился", "rec:dislike" + suffix}
        },
        {
            {"⏭ Следующий", "rec:next" + suffix},
            {"⭐ Оценить (1-10)", "rec:rate" + suffix}
        },
        {
            {"🏠 В меню", "rec:menu"}
        }
    };
    return kb;
}

InlineKeyboard KeyboardFactory::createRatingKeyboard() {
    InlineKeyboard kb;
    InlineKeyboardRow row1;
    row1.reserve(5);
    for (int i = 1; i <= 5; ++i) {
        row1.push_back({std::to_string(i), "rate:" + std::to_string(i)});
    }

    InlineKeyboardRow row2;
    row2.reserve(5);
    for (int i = 6; i <= 10; ++i) {
        row2.push_back({std::to_string(i), "rate:" + std::to_string(i)});
    }

    InlineKeyboardRow row3 = {
        {"🔙 Отмена", "rate:cancel"}
    };

    kb.rows = {std::move(row1), std::move(row2), std::move(row3)};
    return kb;
}

} // namespace bot
