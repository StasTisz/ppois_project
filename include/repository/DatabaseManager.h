#pragma once

#include <sqlite3.h>
#include <string>
#include <memory>

namespace repository {

class DatabaseManager {
public:
    DatabaseManager();
    explicit DatabaseManager(const std::string& dbPath);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    DatabaseManager(DatabaseManager&& other) noexcept;
    DatabaseManager& operator=(DatabaseManager&& other) noexcept;

    bool open(const std::string& dbPath);
    void close();
    bool isOpen() const;
    sqlite3* getHandle() const;

    bool initializeSchema();
    bool seedMoviesFromJson(const std::string& jsonFilePath);
    bool execute(const std::string& sql);

private:
    sqlite3* db_{nullptr};
    std::string dbPath_;
};

} // namespace repository
