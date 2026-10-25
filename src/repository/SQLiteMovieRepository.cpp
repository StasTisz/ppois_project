#include "repository/SQLiteMovieRepository.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace repository {

SQLiteMovieRepository::SQLiteMovieRepository(sqlite3* db) : db_(db) {}

std::vector<std::string> SQLiteMovieRepository::parseGenres(const std::string& genresStr) {
    std::vector<std::string> result;
    std::stringstream ss(genresStr);
    std::string item;
    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        item.erase(item.begin(), std::find_if(item.begin(), item.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        item.erase(std::find_if(item.rbegin(), item.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), item.end());
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

std::string SQLiteMovieRepository::joinGenres(const std::vector<std::string>& genres) {
    std::string result;
    for (size_t i = 0; i < genres.size(); ++i) {
        if (i > 0) result += ", ";
        result += genres[i];
    }
    return result;
}

core::Movie SQLiteMovieRepository::extractMovie(sqlite3_stmt* stmt) {
    core::Movie movie;
    movie.id = sqlite3_column_int64(stmt, 0);

    const auto* titleText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    movie.title = titleText ? titleText : "";

    movie.year = sqlite3_column_int(stmt, 2);

    const auto* genresText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    movie.genres = parseGenres(genresText ? genresText : "");

    const auto* directorText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    movie.director = directorText ? directorText : "";

    const auto* descText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    movie.description = descText ? descText : "";

    movie.ratingAvg = sqlite3_column_double(stmt, 6);

    const auto* posterText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    movie.posterUrl = posterText ? posterText : "";

    if (sqlite3_column_count(stmt) > 8) {
        const auto* fileIdText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        movie.posterFileId = fileIdText ? fileIdText : "";
    }

    if (sqlite3_column_count(stmt) > 9) {
        const auto* trailerText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        movie.trailerUrl = trailerText ? trailerText : "";
    }

    // Pre-calculate zero-allocation micro-embedding
    movie.genreMask = movie.getOrComputeGenreMask();

    return movie;
}

std::optional<core::Movie> SQLiteMovieRepository::getById(int64_t id) {
    if (!db_) return std::nullopt;

    const char* sql = "SELECT id, title, year, genres, director, description, rating_avg, poster_url, poster_file_id, trailer_url "
                      "FROM movies WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, id);

    std::optional<core::Movie> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = extractMovie(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<core::Movie> SQLiteMovieRepository::getAll() {
    std::vector<core::Movie> movies;
    if (!db_) return movies;

    const char* sql = "SELECT id, title, year, genres, director, description, rating_avg, poster_url, poster_file_id, trailer_url "
                      "FROM movies ORDER BY id ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return movies;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        movies.push_back(extractMovie(stmt));
    }

    sqlite3_finalize(stmt);
    return movies;
}

std::vector<core::Movie> SQLiteMovieRepository::getTopRated(size_t limit) {
    std::vector<core::Movie> movies;
    if (!db_) return movies;

    const char* sql = "SELECT id, title, year, genres, director, description, rating_avg, poster_url, poster_file_id, trailer_url "
                      "FROM movies ORDER BY rating_avg DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return movies;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(limit));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        movies.push_back(extractMovie(stmt));
    }

    sqlite3_finalize(stmt);
    return movies;
}

std::vector<core::Movie> SQLiteMovieRepository::getByGenre(const std::string& genre, size_t limit) {
    std::vector<core::Movie> movies;
    if (!db_) return movies;

    const char* sql = "SELECT id, title, year, genres, director, description, rating_avg, poster_url, poster_file_id, trailer_url "
                      "FROM movies WHERE genres LIKE ? ORDER BY rating_avg DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return movies;
    }

    std::string pattern = "%" + genre + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(limit));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        movies.push_back(extractMovie(stmt));
    }

    sqlite3_finalize(stmt);
    return movies;
}

bool SQLiteMovieRepository::addMovie(const core::Movie& movie) {
    if (!db_) return false;

    const char* sql = "INSERT OR REPLACE INTO movies "
                      "(id, title, year, genres, director, description, rating_avg, poster_url, poster_file_id, trailer_url) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string genresStr = joinGenres(movie.genres);

    if (movie.id > 0) {
        sqlite3_bind_int64(stmt, 1, movie.id);
    } else {
        sqlite3_bind_null(stmt, 1);
    }
    sqlite3_bind_text(stmt, 2, movie.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, movie.year);
    sqlite3_bind_text(stmt, 4, genresStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, movie.director.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, movie.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 7, movie.ratingAvg);
    sqlite3_bind_text(stmt, 8, movie.posterUrl.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, movie.posterFileId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, movie.trailerUrl.c_str(), -1, SQLITE_TRANSIENT);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool SQLiteMovieRepository::updatePosterFileId(int64_t movieId, const std::string& fileId) {
    if (!db_) return false;

    const char* sql = "UPDATE movies SET poster_file_id = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, fileId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, movieId);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

size_t SQLiteMovieRepository::count() {
    if (!db_) return 0;

    const char* sql = "SELECT COUNT(*) FROM movies;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        total = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return total;
}


std::vector<core::Movie> SQLiteMovieRepository::findByTitle(const std::string& query, size_t limit) {
    std::vector<core::Movie> movies;
    if (!db_ || query.empty()) return movies;

    const char* sql = "SELECT id, title, year, genres, director, description, rating_avg, poster_url, poster_file_id, trailer_url "
                      "FROM movies WHERE title LIKE ? COLLATE NOCASE "
                      "ORDER BY rating_avg DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return movies;
    }

    std::string pattern = "%" + query + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(limit));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        movies.push_back(extractMovie(stmt));
    }

    sqlite3_finalize(stmt);
    return movies;
}

} // namespace repository
