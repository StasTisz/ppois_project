#include "bot/KeyboardFactory.h"
#include "bot/BotConstants.h"

#include <algorithm>

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
    return InlineKeyboard{{
        {
            {"🎬 Рекомендации", "menu:recommend"},
            {"🎭 Жанры", "menu:genres"}
        },
        {
            {"📜 История", "menu:history"},
            {"ℹ️ Помощь", "menu:help"}
        }
    }};
}

InlineButton KeyboardFactory::createGenreButton(
    const std::string& genre, const std::vector<std::string>& selectedGenres) {
    bool isSelected = std::find(selectedGenres.begin(), selectedGenres.end(), genre) != selectedGenres.end();
    std::string text = isSelected ? ("✅ " + genre) : genre;
    return InlineButton{std::move(text), "genre:toggle:" + genre, ""};
}

InlineKeyboard KeyboardFactory::createGenreSelectionKeyboard(
    const std::vector<std::string>& selectedGenres,
    const std::vector<std::string>& availableGenres) {
    InlineKeyboard kb;
    InlineKeyboardRow currentRow;

    for (const auto& genre : availableGenres) {
        currentRow.push_back(createGenreButton(genre, selectedGenres));
        if (currentRow.size() == kGenresPerRow) {
            kb.rows.push_back(std::move(currentRow));
            currentRow.clear();
        }
    }
    if (!currentRow.empty()) {
        kb.rows.push_back(std::move(currentRow));
    }
    kb.rows.push_back({{"✔️ Готово", "genre:confirm", ""}});
    return kb;
}

InlineKeyboard KeyboardFactory::createMovieReactionKeyboard(int64_t movieId, const std::string& trailerUrl) {
    std::string suffix = (movieId > 0) ? (":" + std::to_string(movieId)) : "";
    InlineKeyboard kb;
    kb.rows.push_back({
        {"👍 Понравился", "rec:like" + suffix, ""},
        {"👎 Не понравился", "rec:dislike" + suffix, ""}
    });
    kb.rows.push_back({
        {"⏭ Следующий", "rec:next" + suffix, ""},
        {"⭐ Оценить (1-10)", "rec:rate" + suffix, ""}
    });
    if (!trailerUrl.empty()) {
        kb.rows.push_back({{"🎬 Смотреть трейлер", "", trailerUrl}});
    }
    kb.rows.push_back({{"🏠 В меню", "rec:menu", ""}});
    return kb;
}

InlineKeyboardRow KeyboardFactory::createRatingRow(int from, int to) {
    InlineKeyboardRow row;
    row.reserve(static_cast<size_t>(to - from + 1));
    for (int i = from; i <= to; ++i) {
        row.push_back({std::to_string(i), "rate:" + std::to_string(i), ""});
    }
    return row;
}

InlineKeyboard KeyboardFactory::createRatingKeyboard() {
    InlineKeyboard kb;
    kb.rows.push_back(createRatingRow(1, 5));
    kb.rows.push_back(createRatingRow(6, 10));
    kb.rows.push_back({{"🔙 Отмена", "rate:cancel", ""}});
    return kb;
}

} // namespace bot
