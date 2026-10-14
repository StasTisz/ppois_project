#pragma once

#include "core/User.h"
#include "core/Rating.h"
#include <vector>
#include <optional>
#include <string>

namespace repository {

class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    virtual std::optional<core::User> getById(int64_t telegramId) = 0;
    virtual bool saveUser(const core::User& user) = 0;

    virtual bool addRating(const core::Rating& rating) = 0;
    virtual std::vector<core::Rating> getUserRatings(int64_t userId) = 0;
    virtual std::vector<int64_t> getRatedMovieIds(int64_t userId) = 0;

    virtual std::vector<std::string> getFavoriteGenres(int64_t userId) = 0;
    virtual bool setFavoriteGenres(int64_t userId, const std::vector<std::string>& genres) = 0;
};

} // namespace repository
