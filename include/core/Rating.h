#pragma once

#include <cstdint>

namespace core {

struct Rating {
    int64_t id{0};
    int64_t userId{0};
    int64_t movieId{0};
    int score{0}; // e.g. 1 to 10
    int64_t timestamp{0};
};

} // namespace core
