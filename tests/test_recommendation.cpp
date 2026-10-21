#include <gtest/gtest.h>
#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"
#include "recommendation/PopularityRecommendationStrategy.h"

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
