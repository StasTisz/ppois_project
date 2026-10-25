#include "repository/DatabaseManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

namespace repository {

DatabaseManager::DatabaseManager() = default;

DatabaseManager::DatabaseManager(const std::string& dbPath) {
    open(dbPath);
}

DatabaseManager::~DatabaseManager() {
    close();
}

DatabaseManager::DatabaseManager(DatabaseManager&& other) noexcept
    : db_(other.db_), dbPath_(std::move(other.dbPath_)) {
    other.db_ = nullptr;
}

DatabaseManager& DatabaseManager::operator=(DatabaseManager&& other) noexcept {
    if (this != &other) {
        close();
        db_ = other.db_;
        dbPath_ = std::move(other.dbPath_);
        other.db_ = nullptr;
    }
    return *this;
}

bool DatabaseManager::open(const std::string& dbPath) {
    close();
    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[DatabaseManager] Failed to open SQLite database at: " 
                  << dbPath << " - " << sqlite3_errmsg(db_) << "\n";
        close();
        return false;
    }
    dbPath_ = dbPath;
    execute("PRAGMA foreign_keys = ON;");
    return true;
}

void DatabaseManager::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    dbPath_.clear();
}

bool DatabaseManager::isOpen() const {
    return db_ != nullptr;
}

sqlite3* DatabaseManager::getHandle() const {
    return db_;
}

bool DatabaseManager::execute(const std::string& sql) {
    if (!db_) return false;
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[DatabaseManager] SQL error: " 
                  << (errMsg ? errMsg : "Unknown error") << " on: " << sql << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool DatabaseManager::initializeSchema() {
    if (!db_) return false;

    const std::string schemaSql = R"(
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS movies (
            id INTEGER PRIMARY KEY,
            title TEXT NOT NULL,
            year INTEGER,
            genres TEXT,
            director TEXT,
            description TEXT,
            rating_avg REAL,
            poster_url TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_movies_rating ON movies(rating_avg DESC);

        CREATE TABLE IF NOT EXISTS users (
            telegram_id INTEGER PRIMARY KEY,
            username TEXT,
            state_id INTEGER DEFAULT 0,
            created_at INTEGER
        );

        CREATE TABLE IF NOT EXISTS ratings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            movie_id INTEGER NOT NULL,
            score INTEGER NOT NULL,
            timestamp INTEGER,
            FOREIGN KEY(user_id) REFERENCES users(telegram_id) ON DELETE CASCADE,
            FOREIGN KEY(movie_id) REFERENCES movies(id) ON DELETE CASCADE,
            UNIQUE(user_id, movie_id)
        );

        CREATE INDEX IF NOT EXISTS idx_ratings_user ON ratings(user_id);
        CREATE INDEX IF NOT EXISTS idx_ratings_movie ON ratings(movie_id);

        CREATE TABLE IF NOT EXISTS favorite_genres (
            user_id INTEGER NOT NULL,
            genre TEXT NOT NULL,
            PRIMARY KEY(user_id, genre),
            FOREIGN KEY(user_id) REFERENCES users(telegram_id) ON DELETE CASCADE
        );
    )";

    return execute(schemaSql);
}

bool DatabaseManager::seedMoviesFromJson(const std::string& jsonFilePath) {
    if (!db_) return false;

    // Check if table already contains data
    sqlite3_stmt* checkStmt = nullptr;
    const char* countSql = "SELECT COUNT(*) FROM movies;";
    if (sqlite3_prepare_v2(db_, countSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(checkStmt, 0);
            sqlite3_finalize(checkStmt);
            if (count > 0) {
                // Table already has movies, skip seeding
                return true;
            }
        } else {
            sqlite3_finalize(checkStmt);
        }
    }

    std::ifstream file(jsonFilePath);
    if (!file.is_open()) {
        std::cerr << "[DatabaseManager] Cannot open seed dataset file: " << jsonFilePath << "\n";
        return false;
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const std::exception& e) {
        std::cerr << "[DatabaseManager] Error parsing seed JSON: " << e.what() << "\n";
        return false;
    }

    if (!root.is_array()) {
        std::cerr << "[DatabaseManager] Expected an array of movies in: " << jsonFilePath << "\n";
        return false;
    }

    execute("BEGIN TRANSACTION;");

    const char* insertSql = R"(
        INSERT OR REPLACE INTO movies 
        (id, title, year, genres, director, description, rating_avg, poster_url) 
        VALUES (?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[DatabaseManager] Failed to prepare insert statement: " 
                  << sqlite3_errmsg(db_) << "\n";
        execute("ROLLBACK;");
        return false;
    }

    for (const auto& item : root) {
        int64_t id = item.value("id", 0);
        std::string title = item.value("title", "");
        int year = item.value("year", 0);
        std::string director = item.value("director", "");
        std::string description = item.value("description", "");
        double ratingAvg = item.value("rating_avg", 0.0);
        std::string posterUrl = item.value("poster_url", "");

        // Format genres as comma-separated string
        std::string genresStr;
        if (item.contains("genres") && item["genres"].is_array()) {
            for (size_t i = 0; i < item["genres"].size(); ++i) {
                if (i > 0) genresStr += ", ";
                genresStr += item["genres"][i].get<std::string>();
            }
        }

        sqlite3_bind_int64(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, year);
        sqlite3_bind_text(stmt, 4, genresStr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, director.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 7, ratingAvg);
        sqlite3_bind_text(stmt, 8, posterUrl.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "[DatabaseManager] Failed to insert movie id " << id << ": " 
                      << sqlite3_errmsg(db_) << "\n";
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    execute("COMMIT;");
    return true;
}

} // namespace repository
