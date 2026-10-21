#include <gtest/gtest.h>
#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"
#include "recommendation/PopularityRecommendationStrategy.h"
#include "recommendation/ContentBasedRecommendationStrategy.h"

class PopularityStrategyTest : public ::testing::Test {
protected:
    repository::DatabaseManager dbManager;
    std::shared_ptr<repository::SQLiteMovieRepository> movieRepo;
    std::shared_ptr<repository::SQLiteUserRepository> userRepo;
    std::unique_ptr<recommendation::PopularityRecommendationStrategy> strategy;

    void SetUp() override {
        ASSERT_TRUE(dbManager.open(":memory:"));
        ASSERT_TRUE(dbManager.initializeSchema());

        movieRepo = std::make_shared<repository::SQLiteMovieRepository>(dbManager.getHandle());
        userRepo = std::make_shared<repository::SQLiteUserRepository>(dbManager.getHandle());

        // Populate test movies
        movieRepo->addMovie({1, "Top Drama", 2020, {"Drama"}, "Dir A", "Desc", 9.5, ""});
        movieRepo->addMovie({2, "Top Sci-Fi", 2021, {"Sci-Fi"}, "Dir B", "Desc", 9.0, ""});
        movieRepo->addMovie({3, "Good Action", 2019, {"Action"}, "Dir C", "Desc", 8.5, ""});
        movieRepo->addMovie({4, "Another Sci-Fi", 2022, {"Sci-Fi", "Action"}, "Dir D", "Desc", 8.8, ""});
        movieRepo->addMovie({5, "Average Comedy", 2018, {"Comedy"}, "Dir E", "Desc", 7.0, ""});

        strategy = std::make_unique<recommendation::PopularityRecommendationStrategy>(movieRepo, userRepo);
    }

    void TearDown() override {
        strategy.reset();
        movieRepo.reset();
        userRepo.reset();
        dbManager.close();
    }
};

TEST_F(PopularityStrategyTest, StrategyName) {
    EXPECT_FALSE(strategy->getName().empty());
}

TEST_F(PopularityStrategyTest, PureColdStartNoPreferences) {
    int64_t newUserId = 101;
    userRepo->saveUser({newUserId, "new_user", 0, 1700000000});

    // Should return top 3 movies overall sorted by rating descending
    auto recs = strategy->recommend(newUserId, 3);
    ASSERT_EQ(recs.size(), 3);
    EXPECT_EQ(recs[0].id, 1); // 9.5
    EXPECT_EQ(recs[1].id, 2); // 9.0
    EXPECT_EQ(recs[2].id, 4); // 8.8
}

TEST_F(PopularityStrategyTest, ColdStartWithFavoriteGenre) {
    int64_t userId = 102;
    userRepo->saveUser({userId, "scifi_fan", 0, 1700000000});
    userRepo->setFavoriteGenres(userId, {"Sci-Fi"});

    auto recs = strategy->recommend(userId, 2);
    ASSERT_EQ(recs.size(), 2);
    // Should prefer Sci-Fi movies: id 2 (9.0) and id 4 (8.8)
    EXPECT_EQ(recs[0].id, 2);
    EXPECT_EQ(recs[1].id, 4);
}

TEST_F(PopularityStrategyTest, BackfillWhenNotEnoughGenreMovies) {
    int64_t userId = 103;
    userRepo->saveUser({userId, "comedy_fan", 0, 1700000000});
    userRepo->setFavoriteGenres(userId, {"Comedy"});

    // There's only 1 Comedy movie (id 5), but we ask for 3
    auto recs = strategy->recommend(userId, 3);
    ASSERT_EQ(recs.size(), 3);

    // Candidates: id 5 (Comedy, 7.0), plus top rated: id 1 (9.5), id 2 (9.0)
    // Sorted descending by rating: id 1 (9.5), id 2 (9.0), id 5 (7.0)
    EXPECT_EQ(recs[0].id, 1);
    EXPECT_EQ(recs[1].id, 2);
    EXPECT_EQ(recs[2].id, 5);
}

TEST_F(PopularityStrategyTest, ExcludesAlreadyRatedMovies) {
    int64_t userId = 104;
    userRepo->saveUser({userId, "active_user", 0, 1700000000});

    // User already watched and rated the top movie (id 1)
    userRepo->addRating({0, userId, 1, 10, 1700000100});

    auto recs = strategy->recommend(userId, 2);
    ASSERT_EQ(recs.size(), 2);
    // id 1 must NOT be in recommendations
    for (const auto& m : recs) {
        EXPECT_NE(m.id, 1);
    }
    EXPECT_EQ(recs[0].id, 2); // 9.0
    EXPECT_EQ(recs[1].id, 4); // 8.8
}

TEST_F(PopularityStrategyTest, ZeroCountReturnsEmpty) {
    int64_t userId = 105;
    auto recs = strategy->recommend(userId, 0);
    EXPECT_TRUE(recs.empty());
}

// =========================================================================
// Tests for Task 1.4: Content-Based Recommendation Strategy
// =========================================================================

class ContentBasedStrategyTest : public ::testing::Test {
protected:
    repository::DatabaseManager dbManager;
    std::shared_ptr<repository::SQLiteMovieRepository> movieRepo;
    std::shared_ptr<repository::SQLiteUserRepository> userRepo;
    std::unique_ptr<recommendation::ContentBasedRecommendationStrategy> strategy;

    void SetUp() override {
        ASSERT_TRUE(dbManager.open(":memory:"));
        ASSERT_TRUE(dbManager.initializeSchema());

        movieRepo = std::make_shared<repository::SQLiteMovieRepository>(dbManager.getHandle());
        userRepo = std::make_shared<repository::SQLiteUserRepository>(dbManager.getHandle());

        strategy = std::make_unique<recommendation::ContentBasedRecommendationStrategy>(
            movieRepo, userRepo, /*minScoreThreshold=*/7
        );
    }

    void TearDown() override {
        strategy.reset();
        movieRepo.reset();
        userRepo.reset();
        dbManager.close();
    }
};

TEST_F(ContentBasedStrategyTest, JaccardSimilarityCalculation) {
    using Strategy = recommendation::ContentBasedRecommendationStrategy;

    // Identical genres
    EXPECT_DOUBLE_EQ(Strategy::calculateJaccardSimilarity({"Action", "Sci-Fi"}, {"Action", "Sci-Fi"}), 1.0);

    // Completely disjoint
    EXPECT_DOUBLE_EQ(Strategy::calculateJaccardSimilarity({"Action"}, {"Comedy"}), 0.0);

    // Empty
    EXPECT_DOUBLE_EQ(Strategy::calculateJaccardSimilarity({}, {"Action"}), 0.0);

    // Partial overlap: {"Action", "Sci-Fi"} and {"Sci-Fi", "Drama"} -> inter=1, union=3 -> 1/3
    double sim = Strategy::calculateJaccardSimilarity({"Action", "Sci-Fi"}, {"Sci-Fi", "Drama"});
    EXPECT_NEAR(sim, 1.0 / 3.0, 0.0001);
}

TEST_F(ContentBasedStrategyTest, MovieSimilarityBoostsSameDirector) {
    using Strategy = recommendation::ContentBasedRecommendationStrategy;

    core::Movie m1{1, "Interstellar", 2014, {"Sci-Fi", "Adventure"}, "Christopher Nolan", "", 8.7, ""};
    core::Movie m2SameDir{2, "Inception", 2010, {"Sci-Fi", "Action"}, "Christopher Nolan", "", 8.8, ""};
    core::Movie m3DiffDir{3, "The Matrix", 1999, {"Sci-Fi", "Action"}, "Wachowskis", "", 8.7, ""};

    double simSameDir = Strategy::calculateMovieSimilarity(m1, m2SameDir);
    double simDiffDir = Strategy::calculateMovieSimilarity(m1, m3DiffDir);

    // Nolan movie should be significantly more similar to another Nolan movie
    EXPECT_GT(simSameDir, simDiffDir);
}

TEST_F(ContentBasedStrategyTest, RecommendsMoviesSimilarToHighlyRated) {
    // Populate catalog
    movieRepo->addMovie({10, "Interstellar", 2014, {"Sci-Fi", "Adventure"}, "Christopher Nolan", "", 8.7, ""});
    movieRepo->addMovie({11, "Inception", 2010, {"Sci-Fi", "Action"}, "Christopher Nolan", "", 8.8, ""});
    movieRepo->addMovie({12, "Tenet", 2020, {"Sci-Fi", "Action"}, "Christopher Nolan", "", 7.5, ""});
    movieRepo->addMovie({13, "The Notebook", 2004, {"Romance", "Drama"}, "Nick Cassavetes", "", 9.0, ""});
    movieRepo->addMovie({14, "La La Land", 2016, {"Comedy", "Drama", "Music"}, "Damien Chazelle", "", 8.0, ""});

    int64_t userId = 777;
    userRepo->saveUser({userId, "nolan_fan", 0, 1700000000});

    // User gave Interstellar a rating of 10
    userRepo->addRating({0, userId, 10, 10, 1700000100});

    // Recommend 2 movies: should pick Inception and Tenet (same genre + same director), NOT The Notebook
    auto recs = strategy->recommend(userId, 2);
    ASSERT_EQ(recs.size(), 2);

    EXPECT_NE(recs[0].id, 10); // Must exclude already rated
    EXPECT_NE(recs[1].id, 10);

    EXPECT_EQ(recs[0].id, 11); // Inception (Nolan + Sci-Fi + high rating)
    EXPECT_EQ(recs[1].id, 12); // Tenet (Nolan + Sci-Fi)
}

TEST_F(ContentBasedStrategyTest, IgnoresLowRatedMoviesAsSeeds) {
    movieRepo->addMovie({20, "Bad Horror", 2020, {"Horror"}, "Dir A", "", 4.0, ""});
    movieRepo->addMovie({21, "Another Horror", 2021, {"Horror"}, "Dir A", "", 5.0, ""});
    movieRepo->addMovie({22, "Top Movie", 2022, {"Drama"}, "Dir B", "", 9.5, ""});

    int64_t userId = 888;
    userRepo->saveUser({userId, "test", 0, 1700000000});

    // User gave Bad Horror score 3 (below threshold 7)
    userRepo->addRating({0, userId, 20, 3, 1700000100});

    // Because score is below threshold, Bad Horror is NOT used as seed
    // Falls back to overall top unrated movies
    auto recs = strategy->recommend(userId, 1);
    ASSERT_EQ(recs.size(), 1);
    EXPECT_EQ(recs[0].id, 22); // Top Movie
}
