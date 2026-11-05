#pragma once

#include "core/GenreMask.h"
#include <string>
#include <vector>
#include <cstdint>

namespace core {

struct Movie {
    int64_t id{0};
    std::string title;
    int year{0};
    std::vector<std::string> genres;
    std::string director;
    std::string description;
    double ratingAvg{0.0};
    std::string posterUrl;
    std::string posterFileId; // Кэшированный file_id Telegram для мгновенной доставки
    std::string trailerUrl;   // Ссылка на трейлер (YouTube/Кинопоиск)
    mutable uint32_t genreMask{0};    // 32-битный микро-эмбеддинг жанров (Zero-Allocation)

    [[nodiscard]] uint32_t getOrComputeGenreMask() const {
        if (genreMask != 0) {
            return genreMask;
        }
        genreMask = genresToMask(genres);
        return genreMask;
    }

    bool operator==(const Movie& other) const {
        return id == other.id;
    }
};

} // namespace core
