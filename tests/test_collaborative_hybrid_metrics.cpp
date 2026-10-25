#include <gtest/gtest.h>

#include "analytics/MetricsCollector.h"
#include "recommendation/CollaborativeFilteringStrategy.h"
#include "recommendation/ContentBasedRecommendationStrategy.h"
#include "recommendation/HybridRecommendationStrategy.h"
#include "recommendation/PopularityRecommendationStrategy.h"
#include "recommendation/RecommendationService.h"
#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

class AdvancedRecommendationMetricsTest : public ::testing::Test {
protected:
    repository::DatabaseManager dbManager;
    std::shared_ptr<repository::SQLiteMovieRepository> movieRepo;
    std::shared_ptr<repository::SQLiteUserRepository> userRepo;

    void SetUp() override {
        ASSERT_TRUE(dbManager.open(":memory:"));
        ASSERT_TRUE(dbManager.initializeSchema());

        movieRepo = std::make_shared<repository::SQLiteMovieRepository>(dbManager.getHandle());
        userRepo = std::make_shared<repository::SQLiteUserRepository>(dbManager.getHandle());

        // Setup movies with genres and trailer URLs
        core::Movie m1{1, "The Shawshank Redemption", 1994, {"Drama"}, "Frank Darabont", "Desc", 9.3, "poster1.jpg", "", "https://youtube.com/trailer1"};
        core::Movie m2{2, "The Godfather", 1972, {"Crime", "Drama"}, "Francis Ford Coppola", "Desc", 9.2, "poster2.jpg", "", "https://youtube.com/trailer2"};
        core::Movie m3{3, "The Dark Knight", 2008, {"Action", "Crime"}, "Christopher Nolan", "Desc", 9.0, "poster3.jpg", "", "https://youtube.com/trailer3"};
        core::Movie m4{4, "Inception", 2010, {"Action", "Sci-Fi"}, "Christopher Nolan", "Desc", 8.8, "poster4.jpg", "", "https://youtube.com/trailer4"};
        core::Movie m5{5, "Interstellar", 2014, {"Sci-Fi", "Drama"}, "Christopher Nolan", "Desc", 8.7, "poster5.jpg", "", "https://youtube.com/trailer5"};
        core::Movie m6{6, "Pulp Fiction", 1994, {"Crime", "Drama"}, "Quentin Tarantino", "Desc", 8.9, "poster6.jpg", "", "https://youtube.com/trailer6"};
        core::Movie m7{7, "Toy Story", 1995, {"Animation", "Comedy"}, "John Lasseter", "Desc", 8.3, "poster7.jpg", "", "https://youtube.com/trailer7"};

        movieRepo->addMovie(m1);
        movieRepo->addMovie(m2);
        movieRepo->addMovie(m3);
        movieRepo->addMovie(m4);
        movieRepo->addMovie(m5);
        movieRepo->addMovie(m6);
        movieRepo->addMovie(m7);

        // Setup users
        userRepo->saveUser({101, "Alice", 0, 1000});
        userRepo->saveUser({102, "Bob", 0, 1000});
        userRepo->saveUser({103, "Charlie", 0, 1000});
    }

    void TearDown() override {
        movieRepo.reset();
        userRepo.reset();
        dbManager.close();
    }
};

// ============================================================================
// 1. Trailer URL in Database & Search By Title Tests
// ============================================================================

TEST_F(AdvancedRecommendationMetricsTest, TrailerUrlPersistedAndRetrieved) {
    auto m1 = movieRepo->getById(1);
    ASSERT_TRUE(m1.has_value());
    EXPECT_EQ(m1->trailerUrl, "https://youtube.com/trailer1");
    EXPECT_EQ(m1->title, "The Shawshank Redemption");

    auto m3 = movieRepo->getById(3);
    ASSERT_TRUE(m3.has_value());
    EXPECT_EQ(m3->trailerUrl, "https://youtube.com/trailer3");
}

TEST_F(AdvancedRecommendationMetricsTest, SearchByTitleExactAndSubstring) {
    auto res1 = movieRepo->findByTitle("Shawshank");
    ASSERT_EQ(res1.size(), 1u);
    EXPECT_EQ(res1[0].id, 1);

    // Case-insensitive substring search
    auto res2 = movieRepo->findByTitle("dark knight");
    ASSERT_EQ(res2.size(), 1u);
    EXPECT_EQ(res2[0].id, 3);

    // Substring matching Nolan movies or Knight
    auto res3 = movieRepo->findByTitle("the");
    EXPECT_GE(res3.size(), 2u);

    // Non-existing query
    auto resNone = movieRepo->findByTitle("NonExistentMovieQuery123");
    EXPECT_TRUE(resNone.empty());
}

TEST_F(AdvancedRecommendationMetricsTest, RecommendationServiceSearchFacade) {
    recommendation::RecommendationService service(movieRepo, userRepo);
    auto found = service.searchMovies("Inception");
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].id, 4);
    EXPECT_EQ(found[0].title, "Inception");
    EXPECT_EQ(found[0].trailerUrl, "https://youtube.com/trailer4");
}

// ============================================================================
// 2. Collaborative Filtering Strategy Tests
// ============================================================================

TEST_F(AdvancedRecommendationMetricsTest, CollaborativeFilteringColdStart) {
    recommendation::CollaborativeFilteringStrategy cf(movieRepo, userRepo);
    // User 101 has no ratings -> CF returns empty (cold start fallback)
    auto recs = cf.recommend(101, 3);
    EXPECT_TRUE(recs.empty());
}

TEST_F(AdvancedRecommendationMetricsTest, CollaborativeFilteringNeighborPrediction) {
    // Alice (101) likes Dark Knight (3: 10) and Inception (4: 9)
    userRepo->addRating({0, 101, 3, 10, 100});
    userRepo->addRating({0, 101, 4, 9, 101});

    // Bob (102) has very similar taste: Dark Knight (3: 9), Inception (4: 10), and Interstellar (5: 10)
    userRepo->addRating({0, 102, 3, 9, 200});
    userRepo->addRating({0, 102, 4, 10, 201});
    userRepo->addRating({0, 102, 5, 10, 202});

    // Charlie (103) likes completely different things: Toy Story (7: 10)
    userRepo->addRating({0, 103, 7, 10, 300});

    recommendation::CollaborativeFilteringStrategy cf(movieRepo, userRepo, 1);
    auto recs = cf.recommend(101, 3);

    // Alice should be recommended Interstellar (5) because Bob loved it and Bob is similar to Alice
    ASSERT_FALSE(recs.empty());
    EXPECT_EQ(recs[0].id, 5);
    EXPECT_EQ(recs[0].title, "Interstellar");
}

// ============================================================================
// 3. Hybrid Strategy & Exploration vs Exploitation Tests
// ============================================================================

TEST_F(AdvancedRecommendationMetricsTest, HybridStrategyColdStartAndWarm) {
    auto pop = std::make_shared<recommendation::PopularityRecommendationStrategy>(movieRepo, userRepo);
    auto cont = std::make_shared<recommendation::ContentBasedRecommendationStrategy>(movieRepo, userRepo);
    auto collab = std::make_shared<recommendation::CollaborativeFilteringStrategy>(movieRepo, userRepo);

    recommendation::HybridRecommendationStrategy hybrid(movieRepo, userRepo, pop, cont, collab, 0.0);
    EXPECT_EQ(hybrid.getName(), "HybridRecommendation");

    // Cold start: returns top rated movies
    auto coldRecs = hybrid.recommend(101, 3);
    ASSERT_EQ(coldRecs.size(), 3u);

    // Warm user: User rates Action/Sci-Fi movies
    userRepo->addRating({0, 101, 3, 10, 100});
    userRepo->addRating({0, 101, 4, 10, 101});
    auto warmRecs = hybrid.recommend(101, 3);
    ASSERT_FALSE(warmRecs.empty());

    // Should not re-recommend already rated movies (3 and 4)
    for (const auto& m : warmRecs) {
        EXPECT_NE(m.id, 3);
        EXPECT_NE(m.id, 4);
    }
}

TEST_F(AdvancedRecommendationMetricsTest, ExplorationVsExploitationRate) {
    auto pop = std::make_shared<recommendation::PopularityRecommendationStrategy>(movieRepo, userRepo);
    auto cont = std::make_shared<recommendation::ContentBasedRecommendationStrategy>(movieRepo, userRepo);
    auto collab = std::make_shared<recommendation::CollaborativeFilteringStrategy>(movieRepo, userRepo);

    recommendation::HybridRecommendationStrategy hybrid(movieRepo, userRepo, pop, cont, collab, 0.3);
    EXPECT_DOUBLE_EQ(hybrid.getExplorationRate(), 0.3);

    hybrid.setExplorationRate(0.5);
    EXPECT_DOUBLE_EQ(hybrid.getExplorationRate(), 0.5);
}

// ============================================================================
// 4. Metrics Collector (CTR, DAU, Latency) Tests
// ============================================================================

TEST(MetricsCollectorTest, ClickThroughRateCalculation) {
    analytics::MetricsCollector collector;

    EXPECT_DOUBLE_EQ(collector.getCTR(), 0.0);
    EXPECT_EQ(collector.getTotalImpressions(), 0u);

    // Record 10 impressions
    for (int i = 0; i < 10; ++i) {
        collector.recordImpression(100 + i, 1);
    }

    // Record 3 likes and 2 dislikes
    collector.recordLike(100, 1);
    collector.recordLike(101, 1);
    collector.recordLike(102, 1);
    collector.recordDislike(103, 1);
    collector.recordDislike(104, 1);

    EXPECT_EQ(collector.getTotalImpressions(), 10u);
    EXPECT_EQ(collector.getTotalLikes(), 3u);
    EXPECT_EQ(collector.getTotalDislikes(), 2u);

    // CTR = 3 likes / 10 impressions = 0.30 (30%)
    EXPECT_NEAR(collector.getCTR(), 0.30, 0.001);

    auto summary = collector.getSummary();
    EXPECT_NEAR(summary.ctr, 0.30, 0.001);
    EXPECT_FALSE(collector.formatReport().empty());
}

TEST(MetricsCollectorTest, DailyActiveUsersTracking) {
    analytics::MetricsCollector collector;

    collector.recordUserActivity(1);
    collector.recordUserActivity(2);
    collector.recordUserActivity(3);
    collector.recordUserActivity(1); // duplicate active user in same day

    EXPECT_EQ(collector.getDailyActiveUsers(), 3u);
}

TEST(MetricsCollectorTest, AverageLatencyCalculation) {
    analytics::MetricsCollector collector;

    collector.recordRecommendationLatency(std::chrono::microseconds(1000)); // 1.0 ms
    collector.recordRecommendationLatency(std::chrono::microseconds(3000)); // 3.0 ms
    collector.recordRecommendationLatency(std::chrono::microseconds(2000)); // 2.0 ms

    // Total: 6.0 ms over 3 requests -> Avg: 2.0 ms
    EXPECT_NEAR(collector.getAverageLatencyMs(), 2.0, 0.001);
}

TEST_F(AdvancedRecommendationMetricsTest, RecommendationServiceMetricsIntegration) {
    recommendation::RecommendationService service(movieRepo, userRepo);
    auto collector = service.getMetricsCollector();
    ASSERT_NE(collector, nullptr);

    EXPECT_EQ(collector->getSummary().totalRecommendationRequests, 0u);

    // Generate recommendations -> triggers latency and DAU tracking
    auto recs = service.getRecommendations(101, 3);
    EXPECT_FALSE(recs.empty());

    EXPECT_EQ(collector->getSummary().totalRecommendationRequests, 1u);
    EXPECT_GT(collector->getAverageLatencyMs(), 0.0);
    EXPECT_EQ(collector->getDailyActiveUsers(), 1u);

    // User rates movie via service -> records like/dislike in collector
    service.rateMovie(101, 1, 9);  // Like
    service.rateMovie(101, 2, 2);  // Dislike

    EXPECT_EQ(collector->getTotalLikes(), 1u);
    EXPECT_EQ(collector->getTotalDislikes(), 1u);
}
