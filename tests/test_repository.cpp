#include <gtest/gtest.h>
#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"

class RepositoryTest : public ::testing::Test {
protected:
    repository::DatabaseManager dbManager;
    std::unique_ptr<repository::SQLiteMovieRepository> movieRepo;
    std::unique_ptr<repository::SQLiteUserRepository> userRepo;

    void SetUp() override {
        // Use in-memory SQLite database for fast, isolated unit tests
        ASSERT_TRUE(dbManager.open(":memory:"));
        ASSERT_TRUE(dbManager.initializeSchema());

        movieRepo = std::make_unique<repository::SQLiteMovieRepository>(dbManager.getHandle());
        userRepo = std::make_unique<repository::SQLiteUserRepository>(dbManager.getHandle());
    }

    void TearDown() override {
        movieRepo.reset();
        userRepo.reset();
        dbManager.close();
    }
};

TEST_F(RepositoryTest, AddAndGetMovie) {
    core::Movie m;
    m.id = 100;
    m.title = "The Matrix Reloaded";
    m.year = 2003;
    m.genres = {"Action", "Sci-Fi"};
    m.director = "Lana Wachowski, Lilly Wachowski";
    m.description = "Neo and the rebel leaders estimate that they have 72 hours until 250,000 probes discover Zion.";
    m.ratingAvg = 7.2;
    m.posterUrl = "https://example.com/matrix2.jpg";

    EXPECT_TRUE(movieRepo->addMovie(m));
    EXPECT_EQ(movieRepo->count(), 1);

    auto retrieved = movieRepo->getById(100);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->title, "The Matrix Reloaded");
    EXPECT_EQ(retrieved->year, 2003);
    EXPECT_EQ(retrieved->genres.size(), 2);
    EXPECT_EQ(retrieved->genres[0], "Action");
    EXPECT_EQ(retrieved->genres[1], "Sci-Fi");
    EXPECT_DOUBLE_EQ(retrieved->ratingAvg, 7.2);
}

TEST_F(RepositoryTest, GetTopRatedAndByGenre) {
    core::Movie m1{1, "Movie A", 2020, {"Drama"}, "Director A", "Desc A", 8.5, ""};
    core::Movie m2{2, "Movie B", 2021, {"Comedy", "Drama"}, "Director B", "Desc B", 9.1, ""};
    core::Movie m3{3, "Movie C", 2022, {"Action", "Sci-Fi"}, "Director C", "Desc C", 7.8, ""};

    movieRepo->addMovie(m1);
    movieRepo->addMovie(m2);
    movieRepo->addMovie(m3);

    auto top = movieRepo->getTopRated(2);
    ASSERT_EQ(top.size(), 2);
    EXPECT_EQ(top[0].id, 2); // 9.1 rating
    EXPECT_EQ(top[1].id, 1); // 8.5 rating

    auto dramaMovies = movieRepo->getByGenre("Drama", 10);
    ASSERT_EQ(dramaMovies.size(), 2);

    auto actionMovies = movieRepo->getByGenre("Action", 10);
    ASSERT_EQ(actionMovies.size(), 1);
    EXPECT_EQ(actionMovies[0].id, 3);
}

TEST_F(RepositoryTest, EmptyDatabaseQueries) {
    EXPECT_EQ(movieRepo->count(), 0);
    EXPECT_TRUE(movieRepo->getAll().empty());
    EXPECT_TRUE(movieRepo->getTopRated(10).empty());
    EXPECT_TRUE(movieRepo->getByGenre("Sci-Fi", 10).empty());
    EXPECT_FALSE(movieRepo->getById(999).has_value());

    EXPECT_FALSE(userRepo->getById(999).has_value());
    EXPECT_TRUE(userRepo->getUserRatings(999).empty());
    EXPECT_TRUE(userRepo->getFavoriteGenres(999).empty());
    EXPECT_TRUE(userRepo->getRatedMovieIds(999).empty());
}

TEST_F(RepositoryTest, SpecialCharactersAndSQLInjectionSafety) {
    // Tests that strings with quotes, semicolons and SQL keywords are treated purely as data
    core::Movie injectionMovie;
    injectionMovie.id = 500;
    injectionMovie.title = "Robert'); DROP TABLE movies;--";
    injectionMovie.year = 2024;
    injectionMovie.genres = {"Crime", "Drama's finest", "Sci-Fi & Action"};
    injectionMovie.director = "O'Connor & Sons";
    injectionMovie.description = "Test description with \"quotes\" and 'single quotes' and symbols $#@!";
    injectionMovie.ratingAvg = 8.0;
    injectionMovie.posterUrl = "https://example.com/poster?id=1&name=test' OR '1'='1";

    EXPECT_TRUE(movieRepo->addMovie(injectionMovie));
    EXPECT_EQ(movieRepo->count(), 1);

    auto retrieved = movieRepo->getById(500);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->title, "Robert'); DROP TABLE movies;--");
    EXPECT_EQ(retrieved->director, "O'Connor & Sons");
    EXPECT_EQ(retrieved->genres.size(), 3);
    EXPECT_EQ(retrieved->genres[1], "Drama's finest");

    // Table movies still exists and is healthy
    EXPECT_EQ(movieRepo->count(), 1);
}

TEST_F(RepositoryTest, SaveAndGetUser) {
    core::User user;
    user.telegramId = 987654321;
    user.username = "cinema_fan";
    user.stateId = 1;
    user.createdAt = 1700000000;

    EXPECT_TRUE(userRepo->saveUser(user));

    auto retrieved = userRepo->getById(987654321);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->telegramId, 987654321);
    EXPECT_EQ(retrieved->username, "cinema_fan");
    EXPECT_EQ(retrieved->stateId, 1);
}

TEST_F(RepositoryTest, UserFavoriteGenres) {
    int64_t userId = 111222;
    core::User user{userId, "test_user", 0, 1700000000};
    userRepo->saveUser(user);

    std::vector<std::string> genres = {"Sci-Fi", "Action", "Thriller"};
    EXPECT_TRUE(userRepo->setFavoriteGenres(userId, genres));

    auto retrievedGenres = userRepo->getFavoriteGenres(userId);
    EXPECT_EQ(retrievedGenres.size(), 3);

    // Overwrite genres
    std::vector<std::string> newGenres = {"Drama", "Romance"};
    EXPECT_TRUE(userRepo->setFavoriteGenres(userId, newGenres));

    retrievedGenres = userRepo->getFavoriteGenres(userId);
    EXPECT_EQ(retrievedGenres.size(), 2);
}

TEST_F(RepositoryTest, UserRatings) {
    int64_t userId = 555;
    core::User user{userId, "reviewer", 0, 1700000000};
    userRepo->saveUser(user);

    core::Movie m1{1, "Interstellar", 2014, {"Sci-Fi"}, "", "", 8.7, ""};
    core::Movie m2{2, "Inception", 2010, {"Sci-Fi"}, "", "", 8.8, ""};
    movieRepo->addMovie(m1);
    movieRepo->addMovie(m2);

    core::Rating r1{0, userId, 1, 9, 1700000100};
    core::Rating r2{0, userId, 2, 10, 1700000200};

    EXPECT_TRUE(userRepo->addRating(r1));
    EXPECT_TRUE(userRepo->addRating(r2));

    auto ratings = userRepo->getUserRatings(userId);
    ASSERT_EQ(ratings.size(), 2);

    auto ratedIds = userRepo->getRatedMovieIds(userId);
    ASSERT_EQ(ratedIds.size(), 2);

    // Update existing rating
    core::Rating r1Updated{0, userId, 1, 10, 1700000300};
    EXPECT_TRUE(userRepo->addRating(r1Updated));

    ratings = userRepo->getUserRatings(userId);
    ASSERT_EQ(ratings.size(), 2);
    auto it = std::find_if(ratings.begin(), ratings.end(), [](const core::Rating& r) {
        return r.movieId == 1;
    });
    ASSERT_NE(it, ratings.end());
    EXPECT_EQ(it->score, 10);
}

TEST_F(RepositoryTest, ForeignKeyCascadeOnUserDelete) {
    int64_t userId = 999111;
    userRepo->saveUser({userId, "temporary_user", 0, 1700000000});

    movieRepo->addMovie({1, "Test Movie", 2020, {"Drama"}, "Dir", "Desc", 8.0, ""});
    userRepo->addRating({0, userId, 1, 9, 1700000100});
    userRepo->setFavoriteGenres(userId, {"Drama"});

    EXPECT_EQ(userRepo->getUserRatings(userId).size(), 1);
    EXPECT_EQ(userRepo->getFavoriteGenres(userId).size(), 1);

    // Delete user directly from SQLite
    dbManager.execute("DELETE FROM users WHERE telegram_id = 999111;");

    // Foreign key cascade must have removed associated ratings and favorite genres
    EXPECT_TRUE(userRepo->getUserRatings(userId).empty());
    EXPECT_TRUE(userRepo->getFavoriteGenres(userId).empty());
}

TEST_F(RepositoryTest, SeedDatasetFromJson) {
    // Seed real 60-movie dataset
    EXPECT_TRUE(dbManager.seedMoviesFromJson("data/initial_movies.json"));
    EXPECT_EQ(movieRepo->count(), 60);

    // Top movie in dataset is The Shawshank Redemption
    auto top = movieRepo->getTopRated(1);
    ASSERT_EQ(top.size(), 1);
    EXPECT_EQ(top[0].title, "The Shawshank Redemption");
    EXPECT_DOUBLE_EQ(top[0].ratingAvg, 9.3);

    // Idempotent: second seeding does not duplicate records
    EXPECT_TRUE(dbManager.seedMoviesFromJson("data/initial_movies.json"));
    EXPECT_EQ(movieRepo->count(), 60);
}
