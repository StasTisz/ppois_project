#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <bit>
#include <algorithm>

namespace core {

enum class GenreBit : uint32_t {
    Unknown     = 0,
    None        = 0,
    Action      = 1u << 0,
    Adventure   = 1u << 1,
    Animation   = 1u << 2,
    Biography   = 1u << 3,
    Comedy      = 1u << 4,
    Crime       = 1u << 5,
    Documentary = 1u << 6,
    Drama       = 1u << 7,
    Family      = 1u << 8,
    Fantasy     = 1u << 9,
    History     = 1u << 10,
    Horror      = 1u << 11,
    Music       = 1u << 12,
    Musical     = 1u << 12,
    Mystery     = 1u << 13,
    Romance     = 1u << 14,
    SciFi       = 1u << 15,
    Thriller    = 1u << 16,
    War         = 1u << 17,
    Western     = 1u << 18
};

/**
 * @brief Преобразует строковое название жанра в битовый флаг GenreBit.
 */
inline GenreBit stringToGenreBit(std::string_view name) {
    if (name.empty()) return GenreBit::Unknown;

    // Сравнение без учета регистра для надежности
    auto equalsCi = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    };

    if (equalsCi(name, "Action") || name == "Боевик" || name == "боевик") return GenreBit::Action;
    if (equalsCi(name, "Adventure") || name == "Приключения" || name == "приключения") return GenreBit::Adventure;
    if (equalsCi(name, "Animation") || name == "Мультфильм" || name == "мультфильм" || name == "Анимация" || name == "анимация") return GenreBit::Animation;
    if (equalsCi(name, "Biography") || name == "Биография" || name == "биография") return GenreBit::Biography;
    if (equalsCi(name, "Comedy") || name == "Комедия" || name == "комедия") return GenreBit::Comedy;
    if (equalsCi(name, "Crime") || name == "Криминал" || name == "криминал") return GenreBit::Crime;
    if (equalsCi(name, "Documentary") || name == "Документальный" || name == "документальный") return GenreBit::Documentary;
    if (equalsCi(name, "Drama") || name == "Драма" || name == "драма") return GenreBit::Drama;
    if (equalsCi(name, "Family") || name == "Семейный" || name == "семейный") return GenreBit::Family;
    if (equalsCi(name, "Fantasy") || name == "Фэнтези" || name == "фэнтези") return GenreBit::Fantasy;
    if (equalsCi(name, "History") || name == "История" || name == "история") return GenreBit::History;
    if (equalsCi(name, "Horror") || name == "Ужасы" || name == "ужасы") return GenreBit::Horror;
    if (equalsCi(name, "Music") || name == "Музыка" || name == "музыка") return GenreBit::Music;
    if (equalsCi(name, "Mystery") || name == "Детектив" || name == "детектив" || name == "Мистика" || name == "мистика") return GenreBit::Mystery;
    if (equalsCi(name, "Romance") || name == "Мелодрама" || name == "мелодрама" || name == "Романтика" || name == "романтика") return GenreBit::Romance;
    if (equalsCi(name, "Sci-Fi") || equalsCi(name, "SciFi") || name == "Фантастика" || name == "фантастика") return GenreBit::SciFi;
    if (equalsCi(name, "Thriller") || name == "Триллер" || name == "триллер") return GenreBit::Thriller;
    if (equalsCi(name, "War") || name == "Военный" || name == "военный") return GenreBit::War;
    if (equalsCi(name, "Western") || name == "Вестерн" || name == "вестерн") return GenreBit::Western;

    return GenreBit::Unknown;
}

/**
 * @brief Кодирует список жанров в 32-битную маску (микро-эмбеддинг).
 */
inline uint32_t genresToMask(const std::vector<std::string>& genres) {
    uint32_t mask = 0;
    for (const auto& g : genres) {
        mask |= static_cast<uint32_t>(stringToGenreBit(g));
    }
    return mask;
}

/**
 * @brief Вычисляет схожесть Жаккара через побитовые операции за 1 процессорный такт (Zero-Allocation).
 * @param maskA Битовая маска жанров первого фильма.
 * @param maskB Битовая маска жанров второго фильма.
 * @return Значение схожести от 0.0 до 1.0.
 */
inline double calculateJaccardBitwise(uint32_t maskA, uint32_t maskB) {
    if (maskA == 0 || maskB == 0) {
        return 0.0;
    }

    uint32_t intersection = maskA & maskB;
    uint32_t unionMask = maskA | maskB;

    if (unionMask == 0) {
        return 0.0;
    }

    // std::popcount поддерживается нативно в C++20 через инструкцию процессора POPCNT
    return static_cast<double>(std::popcount(intersection)) /
           static_cast<double>(std::popcount(unionMask));
}

} // namespace core
