#pragma once

#include "repository/IUserRepository.h"
#include <sqlite3.h>

namespace repository {

class SQLiteUserRepository : public IUserRepository {
public:
    explicit SQLiteUserRepository(sqlite3* db);
    ~SQLiteUserRepository() override = default;

    std::optional<core::User> getById(int64_t telegramId) override;
    bool saveUser(const core::User& user) override;

    bool addRating(const core::Rating& rating) override;
    std::vector<core::Rating> getUserRatings(int64_t userId) override;
    std::vector<int64_t> getRatedMovieIds(int64_t userId) override;

    std::vector<std::string> getFavoriteGenres(int64_t userId) override;
    bool setFavoriteGenres(int64_t userId, const std::vector<std::string>& genres) override;

private:
    sqlite3* db_{nullptr};
};

} // namespace repository
