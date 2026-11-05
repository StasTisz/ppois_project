#include "repository/SQLiteUserRepository.h"
#include <iostream>

namespace repository {

SQLiteUserRepository::SQLiteUserRepository(sqlite3* db) : db_(db) {}

std::optional<core::User> SQLiteUserRepository::getById(int64_t telegramId) {
    if (!db_) return std::nullopt;

    const char* sql = "SELECT telegram_id, username, state_id, created_at FROM users WHERE telegram_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, telegramId);

    std::optional<core::User> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        core::User user;
        user.telegramId = sqlite3_column_int64(stmt, 0);

        const auto* userText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.username = userText ? userText : "";

        user.stateId = sqlite3_column_int(stmt, 2);
        user.createdAt = sqlite3_column_int64(stmt, 3);
        result = user;
    }

    sqlite3_finalize(stmt);
    return result;
}

bool SQLiteUserRepository::saveUser(const core::User& user) {
    if (!db_) return false;

    const char* sql = "INSERT INTO users (telegram_id, username, state_id, created_at) "
                      "VALUES (?, ?, ?, ?) "
                      "ON CONFLICT(telegram_id) DO UPDATE SET "
                      "username = excluded.username, "
                      "state_id = excluded.state_id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, user.telegramId);
    sqlite3_bind_text(stmt, 2, user.username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, user.stateId);
    sqlite3_bind_int64(stmt, 4, user.createdAt);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool SQLiteUserRepository::addRating(const core::Rating& rating) {
    if (!db_) return false;

    const char* sql = "INSERT INTO ratings (user_id, movie_id, score, timestamp) "
                      "VALUES (?, ?, ?, ?) "
                      "ON CONFLICT(user_id, movie_id) DO UPDATE SET "
                      "score = excluded.score, timestamp = excluded.timestamp;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, rating.userId);
    sqlite3_bind_int64(stmt, 2, rating.movieId);
    sqlite3_bind_int(stmt, 3, rating.score);
    sqlite3_bind_int64(stmt, 4, rating.timestamp);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::vector<core::Rating> SQLiteUserRepository::getUserRatings(int64_t userId) {
    std::vector<core::Rating> ratings;
    if (!db_) return ratings;

    const char* sql = "SELECT id, user_id, movie_id, score, timestamp "
                      "FROM ratings WHERE user_id = ? ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return ratings;
    }

    sqlite3_bind_int64(stmt, 1, userId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        core::Rating r;
        r.id = sqlite3_column_int64(stmt, 0);
        r.userId = sqlite3_column_int64(stmt, 1);
        r.movieId = sqlite3_column_int64(stmt, 2);
        r.score = sqlite3_column_int(stmt, 3);
        r.timestamp = sqlite3_column_int64(stmt, 4);
        ratings.push_back(r);
    }

    sqlite3_finalize(stmt);
    return ratings;
}

std::vector<int64_t> SQLiteUserRepository::getRatedMovieIds(int64_t userId) {
    std::vector<int64_t> movieIds;
    if (!db_) return movieIds;

    const char* sql = "SELECT movie_id FROM ratings WHERE user_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return movieIds;
    }

    sqlite3_bind_int64(stmt, 1, userId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        movieIds.push_back(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return movieIds;
}

std::vector<std::string> SQLiteUserRepository::getFavoriteGenres(int64_t userId) {
    std::vector<std::string> genres;
    if (!db_) return genres;

    const char* sql = "SELECT genre FROM favorite_genres WHERE user_id = ? ORDER BY genre ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return genres;
    }

    sqlite3_bind_int64(stmt, 1, userId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* genreText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (genreText) {
            genres.emplace_back(genreText);
        }
    }

    sqlite3_finalize(stmt);
    return genres;
}

bool SQLiteUserRepository::setFavoriteGenres(int64_t userId, const std::vector<std::string>& genres) {
    if (!db_) return false;

    // Begin transaction
    char* errMsg = nullptr;
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);

    const char* deleteSql = "DELETE FROM favorite_genres WHERE user_id = ?;";
    sqlite3_stmt* delStmt = nullptr;
    if (sqlite3_prepare_v2(db_, deleteSql, -1, &delStmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_bind_int64(delStmt, 1, userId);
    sqlite3_step(delStmt);
    sqlite3_finalize(delStmt);

    const char* insertSql = "INSERT OR IGNORE INTO favorite_genres (user_id, genre) VALUES (?, ?);";
    sqlite3_stmt* insStmt = nullptr;
    if (sqlite3_prepare_v2(db_, insertSql, -1, &insStmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& genre : genres) {
        sqlite3_bind_int64(insStmt, 1, userId);
        sqlite3_bind_text(insStmt, 2, genre.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(insStmt);
        sqlite3_reset(insStmt);
    }
    sqlite3_finalize(insStmt);

    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}


std::vector<core::Rating> SQLiteUserRepository::getAllRatings() {
    std::vector<core::Rating> ratings;
    if (!db_) return ratings;

    const char* sql = "SELECT id, user_id, movie_id, score, timestamp FROM ratings ORDER BY timestamp ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return ratings;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        core::Rating r;
        r.id = sqlite3_column_int64(stmt, 0);
        r.userId = sqlite3_column_int64(stmt, 1);
        r.movieId = sqlite3_column_int64(stmt, 2);
        r.score = sqlite3_column_int(stmt, 3);
        r.timestamp = sqlite3_column_int64(stmt, 4);
        ratings.push_back(r);
    }

    sqlite3_finalize(stmt);
    return ratings;
}

std::vector<int64_t> SQLiteUserRepository::getAllUserIds() {
    std::vector<int64_t> userIds;
    if (!db_) return userIds;

    const char* sql = "SELECT telegram_id FROM users;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return userIds;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        userIds.push_back(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return userIds;
}

} // namespace repository
