#pragma once

#include "repository/IMovieRepository.h"
#include <sqlite3.h>
#include <memory>

namespace repository {

class SQLiteMovieRepository : public IMovieRepository {
public:
    explicit SQLiteMovieRepository(sqlite3* db);
    ~SQLiteMovieRepository() override = default;

    std::optional<core::Movie> getById(int64_t id) override;
    std::vector<core::Movie> getAll() override;
    std::vector<core::Movie> getTopRated(size_t limit) override;
    std::vector<core::Movie> getByGenre(const std::string& genre, size_t limit) override;
    bool addMovie(const core::Movie& movie) override;
    size_t count() override;

private:
    sqlite3* db_{nullptr};

    static core::Movie extractMovie(sqlite3_stmt* stmt);
    static std::vector<std::string> parseGenres(const std::string& genresStr);
    static std::string joinGenres(const std::vector<std::string>& genres);
};

} // namespace repository
