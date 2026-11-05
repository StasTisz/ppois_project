#pragma once

#include "core/Movie.h"
#include <vector>
#include <optional>
#include <string>

namespace repository {

class IMovieRepository {
public:
    virtual ~IMovieRepository() = default;

    virtual std::optional<core::Movie> getById(int64_t id) = 0;
    virtual std::vector<core::Movie> getAll() = 0;
    virtual std::vector<core::Movie> getTopRated(size_t limit) = 0;
    virtual std::vector<core::Movie> getByGenre(const std::string& genre, size_t limit) = 0;
    virtual std::vector<core::Movie> findByTitle(const std::string& query, size_t limit = 10) = 0;
    virtual std::optional<core::Movie> findById(int64_t id) { return getById(id); }
    virtual bool addMovie(const core::Movie& movie) = 0;
    virtual bool updatePosterFileId(int64_t movieId, const std::string& fileId) = 0;
    virtual size_t count() = 0;
};

} // namespace repository
