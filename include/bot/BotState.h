#pragma once

#include <cstdint>
#include <string>

namespace bot {

enum class BotState : int32_t {
    Idle = 0,
    SelectingGenres = 1,
    BrowsingRecommendations = 2,
    RatingMovie = 3
};

inline std::string botStateToString(BotState state) {
    switch (state) {
        case BotState::Idle: return "Idle";
        case BotState::SelectingGenres: return "SelectingGenres";
        case BotState::BrowsingRecommendations: return "BrowsingRecommendations";
        case BotState::RatingMovie: return "RatingMovie";
        default: return "Unknown";
    }
}

} // namespace bot
