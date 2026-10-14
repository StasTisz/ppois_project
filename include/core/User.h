#pragma once

#include <string>
#include <cstdint>

namespace core {

struct User {
    int64_t telegramId{0};
    std::string username;
    int stateId{0};
    int64_t createdAt{0};

    bool operator==(const User& other) const {
        return telegramId == other.telegramId;
    }
};

} // namespace core
