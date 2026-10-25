#include <gtest/gtest.h>

#include "analytics/MetricsCollector.h"
#include "bot/BotStateMachine.h"
#include "bot/IBotMessenger.h"
#include "bot/states/BrowsingRecommendationsState.h"
#include "bot/states/RatingState.h"
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
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// 1. Collaborative Filtering Deep Edge Cases & Math Accuracy
// ============================================================================

TEST(CollaborativeFilteringCoverageTest, CosineSimilarityDisjointProfilesReturnsZero) {
    recommendation::CollaborativeFilteringStrategy cf(nullptr, nullptr);

    // User A rated movies 1 and 2
    std::unordered_map<int64_t, int> ratingsA = {{1, 10}, {2, 9}};
    // User B rated movies 3 and 4 (completely disjoint)
    std::unordered_map<int64_t, int> ratingsB = {{3, 8}, {4, 7}};

    EXPECT_DOUBLE_EQ(cf.calculateCosineSimilarity(ratingsA, ratingsB), 0.0);
}

TEST(CollaborativeFilteringCoverageTest, CosineSimilarityMinCommonRatingsEnforced) {
    // Requires at least 2 common ratings
    recommendation::CollaborativeFilteringStrategy cf(nullptr, nullptr, 2);

    // Share only movie 1
    std::unordered_map<int64_t, int> ratingsA = {{1, 10}, {2, 9}};
    std::unordered_map<int64_t, int> ratingsB = {{1, 10}, {3, 8}};

    EXPECT_DOUBLE_EQ(cf.calculateCosineSimilarity(ratingsA, ratingsB), 0.0);
}

TEST(CollaborativeFilteringCoverageTest, CosineSimilarityEmptyInputsReturnsZero) {
    recommendation::CollaborativeFilteringStrategy cf(nullptr, nullptr);

    std::unordered_map<int64_t, int> emptyMap;
    std::unordered_map<int64_t, int> validMap = {{1, 10}};

    EXPECT_DOUBLE_EQ(cf.calculateCosineSimilarity(emptyMap, validMap), 0.0);
    EXPECT_DOUBLE_EQ(cf.calculateCosineSimilarity(validMap, emptyMap), 0.0);
    EXPECT_DOUBLE_EQ(cf.calculateCosineSimilarity(emptyMap, emptyMap), 0.0);
}

TEST(CollaborativeFilteringCoverageTest, MultiUserWeightedPredictionMath) {
    repository::DatabaseManager db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.initializeSchema());

    auto movieRepo = std::make_shared<repository::SQLiteMovieRepository>(db.getHandle());
    auto userRepo = std::make_shared<repository::SQLiteUserRepository>(db.getHandle());

    // 4 test movies
    movieRepo->addMovie({1, "Seed Movie 1", 2020, {"Drama"}, "Dir", "", 8.0, ""});
    movieRepo->addMovie({2, "Candidate A", 2021, {"Drama"}, "Dir", "", 7.5, ""});
    movieRepo->addMovie({3, "Candidate B", 2022, {"Drama"}, "Dir", "", 7.5, ""});
    movieRepo->addMovie({4, "Unrelated Movie", 2019, {"Comedy"}, "Dir", "", 6.0, ""});

    userRepo->saveUser({1, "Target", 0, 0});
    userRepo->saveUser({2, "Neighbor 1", 0, 0});
    userRepo->saveUser({3, "Neighbor 2", 0, 0});

    // Target rated movie 1 with 10
    userRepo->addRating({0, 1, 1, 10, 100});

    // Neighbor 1 rated Movie 1 (10) -> sim = 1.0, Movie 2 (8), Movie 3 (4)
    userRepo->addRating({0, 2, 1, 10, 200});
    userRepo->addRating({0, 2, 2, 8, 201});
    userRepo->addRating({0, 2, 3, 4, 202});

    // Neighbor 2 rated Movie 1 (10) -> sim = 1.0, Movie 2 (4), Movie 3 (10)
    userRepo->addRating({0, 3, 1, 10, 300});
    userRepo->addRating({0, 3, 2, 4, 301});
    userRepo->addRating({0, 3, 3, 10, 302});

    // Predicted Movie 2: (1.0*8 + 1.0*4) / 2.0 = 6.0
    // Predicted Movie 3: (1.0*4 + 1.0*10) / 2.0 = 7.0
    // Candidate B (Movie 3) should strictly rank higher than Candidate A (Movie 2)!

    recommendation::CollaborativeFilteringStrategy cf(movieRepo, userRepo, 1);
    auto recs = cf.recommend(1, 2);

    ASSERT_EQ(recs.size(), 2u);
    EXPECT_EQ(recs[0].id, 3); // Candidate B (score 7.0)
    EXPECT_EQ(recs[1].id, 2); // Candidate A (score 6.0)
}

TEST(CollaborativeFilteringCoverageTest, TargetUserAlreadyRatedAllNeighborsMovies) {
    repository::DatabaseManager db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.initializeSchema());

    auto movieRepo = std::make_shared<repository::SQLiteMovieRepository>(db.getHandle());
    auto userRepo = std::make_shared<repository::SQLiteUserRepository>(db.getHandle());

    movieRepo->addMovie({1, "Movie 1", 2020, {"Drama"}, "Dir", "", 8.0, ""});
    userRepo->saveUser({1, "User 1", 0, 0});
    userRepo->saveUser({2, "User 2", 0, 0});

    userRepo->addRating({0, 1, 1, 10, 100});
    userRepo->addRating({0, 2, 1, 10, 200});

    recommendation::CollaborativeFilteringStrategy cf(movieRepo, userRepo, 1);
    auto recs = cf.recommend(1, 5);

    // No unrated candidate movies left to recommend
    EXPECT_TRUE(recs.empty());
}

// ============================================================================
// 2. Hybrid Strategy Deep Edge Cases & Serendipity Exploration
// ============================================================================

TEST(HybridStrategyCoverageTest, ZeroCountReturnsEmpty) {
    repository::DatabaseManager db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.initializeSchema());

    auto movieRepo = std::make_shared<repository::SQLiteMovieRepository>(db.getHandle());
    auto userRepo = std::make_shared<repository::SQLiteUserRepository>(db.getHandle());

    auto pop = std::make_shared<recommendation::PopularityRecommendationStrategy>(movieRepo, userRepo);
    auto cont = std::make_shared<recommendation::ContentBasedRecommendationStrategy>(movieRepo, userRepo);
    auto collab = std::make_shared<recommendation::CollaborativeFilteringStrategy>(movieRepo, userRepo);

    recommendation::HybridRecommendationStrategy hybrid(movieRepo, userRepo, pop, cont, collab);
    EXPECT_TRUE(hybrid.recommend(1, 0).empty());
}

TEST(HybridStrategyCoverageTest, ExplorationRateClampingValidation) {
    recommendation::HybridRecommendationStrategy hybrid(nullptr, nullptr, nullptr, nullptr, nullptr, 0.2);

    hybrid.setExplorationRate(-1.5);
    EXPECT_DOUBLE_EQ(hybrid.getExplorationRate(), 0.0);

    hybrid.setExplorationRate(2.5);
    EXPECT_DOUBLE_EQ(hybrid.getExplorationRate(), 1.0);

    hybrid.setExplorationRate(0.35);
    EXPECT_DOUBLE_EQ(hybrid.getExplorationRate(), 0.35);
}

TEST(HybridStrategyCoverageTest, ExplorationInjectsNovelty) {
    repository::DatabaseManager db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.initializeSchema());

    auto movieRepo = std::make_shared<repository::SQLiteMovieRepository>(db.getHandle());
    auto userRepo = std::make_shared<repository::SQLiteUserRepository>(db.getHandle());

    // 5 Sci-Fi movies and 1 high-rated Animation movie
    movieRepo->addMovie({1, "Sci-Fi 1", 2020, {"Sci-Fi"}, "Dir", "", 9.0, ""});
    movieRepo->addMovie({2, "Sci-Fi 2", 2021, {"Sci-Fi"}, "Dir", "", 8.9, ""});
    movieRepo->addMovie({3, "Sci-Fi 3", 2022, {"Sci-Fi"}, "Dir", "", 8.8, ""});
    movieRepo->addMovie({4, "Sci-Fi 4", 2023, {"Sci-Fi"}, "Dir", "", 8.7, ""});
    movieRepo->addMovie({5, "Sci-Fi 5", 2024, {"Sci-Fi"}, "Dir", "", 8.6, ""});
    movieRepo->addMovie({6, "Spirited Animation", 2001, {"Animation", "Family"}, "Miyazaki", "", 8.5, ""});

    userRepo->saveUser({1, "SciFiFan", 0, 0});
    userRepo->addRating({0, 1, 1, 10, 100}); // User loved Sci-Fi 1

    auto pop = std::make_shared<recommendation::PopularityRecommendationStrategy>(movieRepo, userRepo);
    auto cont = std::make_shared<recommendation::ContentBasedRecommendationStrategy>(movieRepo, userRepo);
    auto collab = std::make_shared<recommendation::CollaborativeFilteringStrategy>(movieRepo, userRepo);

    // Exploration rate = 0.4 (out of 3 slots, at least 1 exploration slot for novel genre)
    recommendation::HybridRecommendationStrategy hybrid(movieRepo, userRepo, pop, cont, collab, 0.4);

    auto recs = hybrid.recommend(1, 3);
    ASSERT_EQ(recs.size(), 3u);

    // Verify that the novel genre candidate (Animation id 6) was serendipitously explored
    bool foundNovelGenre = false;
    for (const auto& m : recs) {
        if (m.id == 6) {
            foundNovelGenre = true;
            break;
        }
    }
    EXPECT_TRUE(foundNovelGenre);
}

// ============================================================================
// 3. RecommendationService Mode, Search & Exploration Facade
// ============================================================================

TEST(RecommendationServiceCoverageTest, RecommendationModeToStringCoverage) {
    using namespace recommendation;
    EXPECT_EQ(recommendationModeToString(RecommendationMode::Auto), "Auto");
    EXPECT_EQ(recommendationModeToString(RecommendationMode::Popularity), "Popularity");
    EXPECT_EQ(recommendationModeToString(RecommendationMode::ContentBased), "ContentBased");
    EXPECT_EQ(recommendationModeToString(RecommendationMode::Collaborative), "Collaborative");
    EXPECT_EQ(recommendationModeToString(RecommendationMode::Hybrid), "Hybrid");
    EXPECT_EQ(recommendationModeToString(static_cast<RecommendationMode>(999)), "Unknown");
}

TEST(RecommendationServiceCoverageTest, SearchMoviesEmptyAndCaseInsensitive) {
    repository::DatabaseManager db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.initializeSchema());

    auto movieRepo = std::make_shared<repository::SQLiteMovieRepository>(db.getHandle());
    auto userRepo = std::make_shared<repository::SQLiteUserRepository>(db.getHandle());

    movieRepo->addMovie({1, "Casablanca", 1942, {"Drama", "Romance"}, "Michael Curtiz", "", 8.5, ""});
    movieRepo->addMovie({2, "Cast Away", 2000, {"Adventure", "Drama"}, "Robert Zemeckis", "", 7.8, ""});

    recommendation::RecommendationService service(movieRepo, userRepo);

    // Empty search query
    EXPECT_TRUE(service.searchMovies("").empty());

    // Substring "cas" matches both Casablanca and Cast Away
    auto res = service.searchMovies("cas", 5);
    ASSERT_EQ(res.size(), 2u);
    // Casablanca has higher rating (8.5 > 7.8), must be first
    EXPECT_EQ(res[0].id, 1);
    EXPECT_EQ(res[1].id, 2);

    // Limit check
    auto limited = service.searchMovies("cas", 1);
    ASSERT_EQ(limited.size(), 1u);
    EXPECT_EQ(limited[0].id, 1);
}

TEST(RecommendationServiceCoverageTest, ExplorationRateForwarding) {
    repository::DatabaseManager db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.initializeSchema());

    auto movieRepo = std::make_shared<repository::SQLiteMovieRepository>(db.getHandle());
    auto userRepo = std::make_shared<repository::SQLiteUserRepository>(db.getHandle());

    recommendation::RecommendationService service(movieRepo, userRepo);
    service.setExplorationRate(0.25);
    EXPECT_DOUBLE_EQ(service.getExplorationRate(), 0.25);

    auto hybrid = service.getHybridStrategy();
    ASSERT_NE(hybrid, nullptr);
    auto hybridCast = std::dynamic_pointer_cast<recommendation::HybridRecommendationStrategy>(hybrid);
    ASSERT_NE(hybridCast, nullptr);
    EXPECT_DOUBLE_EQ(hybridCast->getExplorationRate(), 0.25);
}

TEST(RecommendationServiceCoverageTest, ExplicitCollaborativeAndHybridModes) {
    repository::DatabaseManager db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(db.initializeSchema());

    auto movieRepo = std::make_shared<repository::SQLiteMovieRepository>(db.getHandle());
    auto userRepo = std::make_shared<repository::SQLiteUserRepository>(db.getHandle());

    movieRepo->addMovie({1, "Top Movie", 2020, {"Drama"}, "Dir", "", 9.0, ""});
    userRepo->saveUser({10, "User10", 0, 0});

    recommendation::RecommendationService service(movieRepo, userRepo);

    EXPECT_EQ(service.resolveStrategyName(10, recommendation::RecommendationMode::Collaborative), "CollaborativeFiltering");
    EXPECT_EQ(service.resolveStrategyName(10, recommendation::RecommendationMode::Hybrid), "HybridRecommendation");
}

// ============================================================================
// 4. MetricsCollector Stress & Multi-threaded Concurrency Safety
// ============================================================================

TEST(MetricsCollectorCoverageTest, ZeroDivisionSafetyAndReset) {
    analytics::MetricsCollector collector;

    // Zero metrics
    EXPECT_DOUBLE_EQ(collector.getCTR(), 0.0);
    EXPECT_DOUBLE_EQ(collector.getAverageLatencyMs(), 0.0);
    EXPECT_EQ(collector.getDailyActiveUsers(), 0u);
    EXPECT_EQ(collector.getTotalImpressions(), 0u);

    collector.recordImpression(1, 10);
    collector.recordLike(1, 10);
    collector.recordRecommendationLatency(std::chrono::microseconds(5000));
    EXPECT_GT(collector.getCTR(), 0.0);

    collector.reset();
    EXPECT_DOUBLE_EQ(collector.getCTR(), 0.0);
    EXPECT_DOUBLE_EQ(collector.getAverageLatencyMs(), 0.0);
    EXPECT_EQ(collector.getDailyActiveUsers(), 0u);
    EXPECT_EQ(collector.getTotalImpressions(), 0u);
}

TEST(MetricsCollectorCoverageTest, MultithreadedConcurrentStressTest) {
    analytics::MetricsCollector collector;

    constexpr int kNumThreads = 8;
    constexpr int kOperationsPerThread = 200;

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([&collector, t]() {
            for (int i = 0; i < kOperationsPerThread; ++i) {
                int64_t userId = (t * 1000) + i;
                collector.recordImpression(userId, 42);
                if (i % 2 == 0) {
                    collector.recordLike(userId, 42);
                } else {
                    collector.recordDislike(userId, 42);
                }
                collector.recordRecommendationLatency(std::chrono::microseconds(100));
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    constexpr size_t kExpectedImpressions = kNumThreads * kOperationsPerThread;
    EXPECT_EQ(collector.getTotalImpressions(), kExpectedImpressions);
    EXPECT_EQ(collector.getTotalLikes(), kExpectedImpressions / 2);
    EXPECT_EQ(collector.getTotalDislikes(), kExpectedImpressions / 2);
    EXPECT_DOUBLE_EQ(collector.getCTR(), 0.50);
    EXPECT_EQ(collector.getDailyActiveUsers(), kExpectedImpressions);
}

// ============================================================================
// 5. Bot State Machine & States Metrics Integration
// ============================================================================

namespace {

class DummyMessengerSpy : public bot::IBotMessenger {
public:
    void sendMessage(int64_t, const std::string&) override {}
    void answerCallbackQuery(const std::string&, const std::string& = "") override {}
};

} // namespace

TEST(BotFsmMetricsCoverageTest, StateMachineForwardsImpressionsAndReactions) {
    auto messenger = std::make_shared<DummyMessengerSpy>();
    bot::BotStateMachine fsm(messenger);

    auto collector = std::make_shared<analytics::MetricsCollector>();
    fsm.setMetricsCollector(collector);
    EXPECT_EQ(fsm.getMetricsCollector(), collector);

    fsm.recordImpression(123, 456);
    fsm.recordReaction(123, 456, true);
    fsm.recordReaction(123, 456, false);

    EXPECT_EQ(collector->getTotalImpressions(), 1u);
    EXPECT_EQ(collector->getTotalLikes(), 1u);
    EXPECT_EQ(collector->getTotalDislikes(), 1u);
}

TEST(BotFsmMetricsCoverageTest, RatingStateForwardsReactionToMetrics) {
    auto messenger = std::make_shared<DummyMessengerSpy>();
    bot::BotStateMachine fsm(messenger);

    auto collector = std::make_shared<analytics::MetricsCollector>();
    fsm.setMetricsCollector(collector);

    bot::RatingState state;
    auto& session = fsm.getSession(555, 1234, "tester");
    core::Movie m;
    m.id = 77;
    m.title = "Test Film";
    session.currentMovie = m;

    // Send valid like rating "9"
    bot::IncomingMessage msg;
    msg.userId = 555;
    msg.chatId = 1234;
    msg.text = "9";

    state.handleMessage(fsm, session, msg);

    EXPECT_EQ(collector->getTotalLikes(), 1u);
    EXPECT_EQ(collector->getTotalDislikes(), 0u);

    // Send dislike callback "rate:2"
    bot::IncomingCallback cb;
    cb.userId = 555;
    cb.chatId = 1234;
    cb.data = "rate:2";

    state.handleCallback(fsm, session, cb);

    EXPECT_EQ(collector->getTotalLikes(), 1u);
    EXPECT_EQ(collector->getTotalDislikes(), 1u);
}


TEST(CoverageExpansionTest, BotStateMachineIgnoresStorageChatMessages) {
    auto spy = std::make_shared<RecordingMessenger>();
    bot::BotStateMachine fsm(spy);
    fsm.setStorageChatId(-1005310490407LL);

    // Message from storage chat should be ignored
    bot::IncomingMessage msg;
    msg.userId = 999;
    msg.chatId = -1005310490407LL;
    msg.text = "Random photo caption or text";
    fsm.processMessage(msg);

    EXPECT_TRUE(spy->messages.empty());

    // Callback from storage chat should be ignored
    bot::IncomingCallback cb;
    cb.userId = 999;
    cb.chatId = -1005310490407LL;
    cb.data = "genre:toggle:Action";
    fsm.processCallback(cb);

    EXPECT_TRUE(spy->messages.empty());
}

TEST(CoverageExpansionTest, GenreSelectionValidatesAndRejectsUnknownInput) {
    auto spy = std::make_shared<RecordingMessenger>();
    bot::BotStateMachine fsm(spy);
    auto& session = fsm.getSession(12345, 100);
    session.state = bot::BotState::SelectingGenres;

    bot::GenreSelectionState state;

    // Unknown genre string
    bot::IncomingMessage msgBad;
    msgBad.userId = 12345;
    msgBad.chatId = 100;
    msgBad.text = "some_random_nonsense";

    state.handleMessage(fsm, session, msgBad);
    EXPECT_TRUE(session.selectedGenres.empty());

    // Valid Russian genre string
    bot::IncomingMessage msgGood;
    msgGood.userId = 12345;
    msgGood.chatId = 100;
    msgGood.text = "Комедия";

    state.handleMessage(fsm, session, msgGood);
    EXPECT_EQ(session.selectedGenres.size(), 1u);
    EXPECT_EQ(session.selectedGenres[0], "Комедия");
}

TEST(CoverageExpansionTest, GenreMaskRussianSynonymsRecognition) {
    EXPECT_EQ(core::stringToGenreBit("Боевик"), core::GenreBit::Action);
    EXPECT_EQ(core::stringToGenreBit("комедия"), core::GenreBit::Comedy);
    EXPECT_EQ(core::stringToGenreBit("Фантастика"), core::GenreBit::SciFi);
    EXPECT_EQ(core::stringToGenreBit("ужасы"), core::GenreBit::Horror);
    EXPECT_EQ(core::stringToGenreBit("НепонятныйЖанр"), core::GenreBit::Unknown);
}

TEST(CoverageExpansionTest, CommandDispatcherHandlesMenuCommand) {
    auto spy = std::make_shared<RecordingMessenger>();
    bot::BotStateMachine fsm(spy);
    auto& session = fsm.getSession(1001, 2002);
    session.state = bot::BotState::SelectingGenres;

    bot::CommandDispatcher dispatcher(fsm, spy);
    bot::IncomingMessage msg;
    msg.userId = 1001;
    msg.chatId = 2002;
    msg.text = "/menu";

    bool handled = dispatcher.dispatch(msg);
    EXPECT_TRUE(handled);
    EXPECT_EQ(session.state, bot::BotState::Idle);
}
