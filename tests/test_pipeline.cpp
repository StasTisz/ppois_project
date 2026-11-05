#include <gtest/gtest.h>
#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"
#include "recommendation/RecommendationService.h"

class FullBackendPipelineTest : public ::testing::Test {
protected:
    repository::DatabaseManager dbManager;
    std::shared_ptr<repository::SQLiteMovieRepository> movieRepo;
    std::shared_ptr<repository::SQLiteUserRepository> userRepo;
    std::unique_ptr<recommendation::RecommendationService> service;

    void SetUp() override {
        ASSERT_TRUE(dbManager.open(":memory:"));
        ASSERT_TRUE(dbManager.initializeSchema());

        // Seed with actual 60-movie dataset
        ASSERT_TRUE(dbManager.seedMoviesFromJson("data/initial_movies.json"));

        movieRepo = std::make_shared<repository::SQLiteMovieRepository>(dbManager.getHandle());
        userRepo = std::make_shared<repository::SQLiteUserRepository>(dbManager.getHandle());
        service = std::make_unique<recommendation::RecommendationService>(movieRepo, userRepo);
    }

    void TearDown() override {
        service.reset();
        movieRepo.reset();
        userRepo.reset();
        dbManager.close();
    }
};

TEST_F(FullBackendPipelineTest, CompleteUserLifecycleAndRecommendationTransition) {
    // 1. Verify dataset integrity
    ASSERT_EQ(movieRepo->count(), 60);

    // 2. User registers
    int64_t userId = 424242;
    core::User user{userId, "stanley_kubrick_fan", 0, 1700000000};
    ASSERT_TRUE(userRepo->saveUser(user));

    // 3. User selects favorite genres: Crime & Drama
    ASSERT_TRUE(service->setFavoriteGenres(userId, {"Crime", "Drama"}));
    auto favs = service->getFavoriteGenres(userId);
    EXPECT_EQ(favs.size(), 2);

    // 4. Initial recommendation (Cold Start)
    // Mode Auto must resolve to Popularity because user has 0 ratings
    EXPECT_EQ(service->resolveStrategyName(userId, recommendation::RecommendationMode::Auto), "Cold-Start / Popularity");

    auto coldStartRecs = service->getRecommendations(userId, 3, recommendation::RecommendationMode::Auto);
    ASSERT_EQ(coldStartRecs.size(), 3);

    // Top Crime/Drama in dataset: The Shawshank Redemption (9.3), The Godfather (9.2), The Dark Knight (9.0) / Pulp Fiction (8.9)
    EXPECT_EQ(coldStartRecs[0].title, "The Shawshank Redemption");
    EXPECT_EQ(coldStartRecs[1].title, "The Godfather");

    // 5. User rates "The Godfather" (id 2) with 10/10
    ASSERT_TRUE(service->rateMovie(userId, 2, 10));

    // Verify rating persisted
    auto ratings = service->getUserRatings(userId);
    ASSERT_EQ(ratings.size(), 1);
    EXPECT_EQ(ratings[0].movieId, 2);
    EXPECT_EQ(ratings[0].score, 10);

    // 6. Next recommendation transition (Content-Based)
    // Mode Auto must now automatically resolve to Content-Based Filtering!
    EXPECT_EQ(service->resolveStrategyName(userId, recommendation::RecommendationMode::Auto), "Content-Based Filtering");

    auto contentRecs = service->getRecommendations(userId, 3, recommendation::RecommendationMode::Auto);
    ASSERT_EQ(contentRecs.size(), 3);

    // The Godfather (id 2) MUST NOT be in recommendations (already watched & rated)
    for (const auto& m : contentRecs) {
        EXPECT_NE(m.id, 2);
        EXPECT_NE(m.title, "The Godfather");
    }

    // Top recommended should be other mafia / crime dramas, e.g. Goodfellas, Pulp Fiction, The Departed
    bool foundCrimeDrama = false;
    for (const auto& m : contentRecs) {
        for (const auto& g : m.genres) {
            if (g == "Crime" || g == "Drama") {
                foundCrimeDrama = true;
                break;
            }
        }
    }
    EXPECT_TRUE(foundCrimeDrama);

    // 7. Test "Similar Movies" functionality (e.g. for "Interstellar" id 10)
    auto similarToInterstellar = service->getSimilarMovies(10, 3);
    ASSERT_EQ(similarToInterstellar.size(), 3);

    // Interstellar is Nolan Sci-Fi. Similar movies should include Inception (id 5, Nolan Sci-Fi)
    EXPECT_NE(similarToInterstellar[0].id, 10);
    EXPECT_EQ(similarToInterstellar[0].director, "Christopher Nolan");
}
