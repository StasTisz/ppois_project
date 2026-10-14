#pragma once

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

    bool operator==(const Movie& other) const {
        return id == other.id;
    }
};

} // namespace core
