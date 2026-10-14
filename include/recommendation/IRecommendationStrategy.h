#pragma once

#include "core/Movie.h"
#include <vector>
#include <string>
#include <cstdint>

namespace recommendation {

class IRecommendationStrategy {
public:
    virtual ~IRecommendationStrategy() = default;

    virtual std::string getName() const = 0;
    virtual std::vector<core::Movie> recommend(int64_t userId, size_t count) = 0;
};

} // namespace recommendation
