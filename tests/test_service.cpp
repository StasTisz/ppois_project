#include <gtest/gtest.h>
#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"
#include "recommendation/RecommendationService.h"

class RecommendationServiceTest : public ::testing::Test {
protected:
    repository::DatabaseManager dbManager;
    std::shared_ptr<repository::SQLiteMovieRepository> movieRepo;
    std::shared_ptr<repository::SQLiteUserRepository> userRepo;
    std::unique_ptr<recommendation::RecommendationService> service;

    void SetUp() override {
        ASSERT_TRUE(dbManager.open(":memory:"));
        ASSERT_TRUE(dbManager.initializeSchema());

        movieRepo = std::make_shared<repository::SQLiteMovieRepository>(dbManager.getHandle());
        userRepo = std::make_shared<repository::SQLiteUserRepository>(dbManager.getHandle());

        // Setup test catalog
        movieRepo->addMovie({1, "Interstellar", 2014, {"Sci-Fi", "Adventure"}, "Christopher Nolan", "", 8.7, ""});
        movieRepo->addMovie({2, "Inception", 2010, {"Sci-Fi", "Action"}, "Christopher Nolan", "", 8.8, ""});
        movieRepo->addMovie({3, "Dunkirk", 2017, {"Action", "Drama", "War"}, "Christopher Nolan", "", 7.8, ""});
        movieRepo->addMovie({4, "The Dark Knight", 2008, {"Action", "Crime", "Drama"}, "Christopher Nolan", "", 9.0, ""});
        movieRepo->addMovie({5, "Pulp Fiction", 1994, {"Crime", "Drama"}, "Quentin Tarantino", "", 8.9, ""});
        movieRepo->addMovie({6, "Amelie", 2001, {"Comedy", "Romance"}, "Jean-Pierre Jeunet", "", 8.3, ""});

        service = std::make_unique<recommendation::RecommendationService>(movieRepo, userRepo);
    }

    void TearDown() override {
        service.reset();
        movieRepo.reset();
        userRepo.reset();
        dbManager.close();
    }
};

TEST_F(RecommendationServiceTest, AutoSelectsPopularityForNewUser) {
    int64_t newUserId = 1001;
    userRepo->saveUser({newUserId, "newbie", 0, 1700000000});

    EXPECT_FALSE(service->hasSufficientHistory(newUserId));

    std::string strategyName = service->resolveStrategyName(newUserId, recommendation::RecommendationMode::Auto);
    EXPECT_NE(strategyName.find("Popularity"), std::string::npos);

    auto recs = service->getRecommendations(newUserId, 3, recommendation::RecommendationMode::Auto);
    ASSERT_EQ(recs.size(), 3);
    // Highest rated overall: The Dark Knight (9.0), Pulp Fiction (8.9), Inception (8.8)
    EXPECT_EQ(recs[0].id, 4);
    EXPECT_EQ(recs[1].id, 5);
    EXPECT_EQ(recs[2].id, 2);
}

TEST_F(RecommendationServiceTest, AutoSelectsContentBasedForActiveUser) {
    int64_t activeUserId = 1002;
    userRepo->saveUser({activeUserId, "active_user", 0, 1700000000});

    // Rate Interstellar with 10/10
    EXPECT_TRUE(service->rateMovie(activeUserId, 1, 10));
    EXPECT_TRUE(service->hasSufficientHistory(activeUserId));

    std::string strategyName = service->resolveStrategyName(activeUserId, recommendation::RecommendationMode::Auto);
    EXPECT_NE(strategyName.find("Content-Based"), std::string::npos);

    auto recs = service->getRecommendations(activeUserId, 2, recommendation::RecommendationMode::Auto);
    ASSERT_EQ(recs.size(), 2);
    // Should recommend other Nolan Sci-Fi / Action movies, e.g. Inception (2) and The Dark Knight (4) or Dunkirk (3)
    // Strictly not Interstellar (1) because it's rated
    EXPECT_NE(recs[0].id, 1);
    EXPECT_NE(recs[1].id, 1);
    EXPECT_EQ(recs[0].id, 2); // Inception: same director + Sci-Fi
}

TEST_F(RecommendationServiceTest, ExplicitModeOverridesAuto) {
    int64_t userId = 1003;
    userRepo->saveUser({userId, "fan", 0, 1700000000});
    service->rateMovie(userId, 1, 9); // User has rating

    // Force Popularity mode
    auto popStrategyName = service->resolveStrategyName(userId, recommendation::RecommendationMode::Popularity);
    EXPECT_NE(popStrategyName.find("Popularity"), std::string::npos);

    // Force ContentBased mode on a user without ratings
    int64_t emptyUserId = 1004;
    auto cbStrategyName = service->resolveStrategyName(emptyUserId, recommendation::RecommendationMode::ContentBased);
    EXPECT_NE(cbStrategyName.find("Content-Based"), std::string::npos);
}

TEST_F(RecommendationServiceTest, RateMovieValidation) {
    int64_t userId = 1005;
    userRepo->saveUser({userId, "rater", 0, 1700000000});

    // Valid rating
    EXPECT_TRUE(service->rateMovie(userId, 1, 8));

    // Invalid score (< 1 or > 10)
    EXPECT_FALSE(service->rateMovie(userId, 1, 0));
    EXPECT_FALSE(service->rateMovie(userId, 1, 11));

    // Non-existent movie ID
    EXPECT_FALSE(service->rateMovie(userId, 9999, 8));
}

TEST_F(RecommendationServiceTest, GetSimilarMovies) {
    // Look for movies similar to Inception (id 2, Nolan, Sci-Fi/Action, 2010)
    auto similar = service->getSimilarMovies(2, 2);
    ASSERT_EQ(similar.size(), 2);

    // Target movie must NOT be in its own similar list
    EXPECT_NE(similar[0].id, 2);
    EXPECT_NE(similar[1].id, 2);

    // Top matches should be Interstellar (1) or The Dark Knight (4) / Dunkirk (3) due to Nolan + genre overlap
    EXPECT_EQ(similar[0].director, "Christopher Nolan");
}

TEST_F(RecommendationServiceTest, FavoriteGenresManagement) {
    int64_t userId = 1006;
    userRepo->saveUser({userId, "genre_lover", 0, 1700000000});

    EXPECT_TRUE(service->setFavoriteGenres(userId, {"Crime", "Drama"}));

    auto genres = service->getFavoriteGenres(userId);
    EXPECT_EQ(genres.size(), 2);
}

TEST_F(RecommendationServiceTest, CustomStrategyInjection) {
    class DummyStrategy : public recommendation::IRecommendationStrategy {
    public:
        std::string getName() const override { return "Mock Dummy Strategy"; }
        std::vector<core::Movie> recommend(int64_t, size_t) override {
            return {core::Movie{99, "Dummy Movie", 2026, {"Mock"}, "Mock", "Mock", 9.9, ""}};
        }
    };

    int64_t userId = 1007;
    service->setCustomStrategy(std::make_shared<DummyStrategy>());

    EXPECT_EQ(service->resolveStrategyName(userId), "Mock Dummy Strategy");
    auto recs = service->getRecommendations(userId, 1);
    ASSERT_EQ(recs.size(), 1);
    EXPECT_EQ(recs[0].id, 99);

    service->clearCustomStrategy();
    EXPECT_NE(service->resolveStrategyName(userId), "Mock Dummy Strategy");
}
