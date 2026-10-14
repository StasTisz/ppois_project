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
    virtual bool addMovie(const core::Movie& movie) = 0;
    virtual size_t count() = 0;
};

} // namespace repository
