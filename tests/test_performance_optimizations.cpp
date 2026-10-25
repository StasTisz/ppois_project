#include <gtest/gtest.h>

#include "core/GenreMask.h"
#include "core/Movie.h"
#include "bot/ThreadPool.h"
#include "bot/MovieCardFormatter.h"
#include "bot/IBotMessenger.h"
#include "bot/InlineKeyboard.h"
#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"

#include <atomic>
#include <chrono>
#include <future>
#include <numeric>
#include <string>
#include <vector>

// ============================================================================
// 1. Bitwise Genre Mask Tests (Zero-Allocation Micro-Embeddings)
// ============================================================================

TEST(GenreMaskTest, StringToGenreBitMapping) {
    EXPECT_EQ(core::stringToGenreBit("Action"), core::GenreBit::Action);
    EXPECT_EQ(core::stringToGenreBit("action"), core::GenreBit::Action);
    EXPECT_EQ(core::stringToGenreBit("ACTION"), core::GenreBit::Action);
    EXPECT_EQ(core::stringToGenreBit("Sci-Fi"), core::GenreBit::SciFi);
    EXPECT_EQ(core::stringToGenreBit("Science Fiction"), core::GenreBit::SciFi);
    EXPECT_EQ(core::stringToGenreBit("Drama"), core::GenreBit::Drama);
    EXPECT_EQ(core::stringToGenreBit("Comedy"), core::GenreBit::Comedy);
    EXPECT_EQ(core::stringToGenreBit("Unknown_Genre_123"), core::GenreBit::None);
}

TEST(GenreMaskTest, GenresToMaskVectorConversion) {
    std::vector<std::string> genres = {"Action", "Sci-Fi", "Drama"};
    uint32_t mask = core::genresToMask(genres);

    EXPECT_NE(mask, 0u);
    EXPECT_TRUE(mask & static_cast<uint32_t>(core::GenreBit::Action));
    EXPECT_TRUE(mask & static_cast<uint32_t>(core::GenreBit::SciFi));
    EXPECT_TRUE(mask & static_cast<uint32_t>(core::GenreBit::Drama));
    EXPECT_FALSE(mask & static_cast<uint32_t>(core::GenreBit::Comedy));
    EXPECT_FALSE(mask & static_cast<uint32_t>(core::GenreBit::Horror));
}

TEST(GenreMaskTest, BitwiseJaccardIdentical) {
    uint32_t mask1 = static_cast<uint32_t>(core::GenreBit::Action) | static_cast<uint32_t>(core::GenreBit::Thriller);
    uint32_t mask2 = mask1;

    EXPECT_FLOAT_EQ(core::calculateJaccardBitwise(mask1, mask2), 1.0f);
}

TEST(GenreMaskTest, BitwiseJaccardDisjoint) {
    uint32_t mask1 = static_cast<uint32_t>(core::GenreBit::Action) | static_cast<uint32_t>(core::GenreBit::SciFi);
    uint32_t mask2 = static_cast<uint32_t>(core::GenreBit::Romance) | static_cast<uint32_t>(core::GenreBit::Musical);

    EXPECT_FLOAT_EQ(core::calculateJaccardBitwise(mask1, mask2), 0.0f);
}

TEST(GenreMaskTest, BitwiseJaccardPartialOverlap) {
    // mask1: Action, Sci-Fi, Drama (3 genres)
    // mask2: Sci-Fi, Comedy (2 genres)
    // Intersection: Sci-Fi (1 genre)
    // Union: Action, Sci-Fi, Drama, Comedy (4 genres)
    // Jaccard: 1 / 4 = 0.25f
    uint32_t mask1 = static_cast<uint32_t>(core::GenreBit::Action) |
                     static_cast<uint32_t>(core::GenreBit::SciFi) |
                     static_cast<uint32_t>(core::GenreBit::Drama);

    uint32_t mask2 = static_cast<uint32_t>(core::GenreBit::SciFi) |
                     static_cast<uint32_t>(core::GenreBit::Comedy);

    EXPECT_FLOAT_EQ(core::calculateJaccardBitwise(mask1, mask2), 0.25f);
}

TEST(GenreMaskTest, BitwiseJaccardBothEmpty) {
    EXPECT_FLOAT_EQ(core::calculateJaccardBitwise(0u, 0u), 0.0f);
}

TEST(GenreMaskTest, MovieGenreMaskLazyComputation) {
    core::Movie movie;
    movie.id = 42;
    movie.genres = {"Animation", "Family", "Adventure"};
    EXPECT_EQ(movie.genreMask, 0u);

    uint32_t computed = movie.getOrComputeGenreMask();
    EXPECT_NE(computed, 0u);
    EXPECT_EQ(movie.genreMask, computed);
    // Second call should return cached value
    EXPECT_EQ(movie.getOrComputeGenreMask(), computed);
}

// ============================================================================
// 2. Thread Pool Tests (Asynchronous Non-blocking Architecture)
// ============================================================================

TEST(ThreadPoolTest, ThreadPoolInitializationAndSize) {
    bot::ThreadPool pool(4);
    EXPECT_EQ(pool.size(), 4u);
}

TEST(ThreadPoolTest, ConcurrentTaskExecutionAndFutures) {
    bot::ThreadPool pool(4);
    std::vector<std::future<int>> futures;
    constexpr int kTaskCount = 20;

    for (int i = 0; i < kTaskCount; ++i) {
        futures.push_back(pool.enqueue([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return i * 2;
        }));
    }

    for (int i = 0; i < kTaskCount; ++i) {
        EXPECT_EQ(futures[i].get(), i * 2);
    }
}

TEST(ThreadPoolTest, AtomicCounterAccumulation) {
    bot::ThreadPool pool(4);
    std::atomic<int> counter{0};
    constexpr int kTaskCount = 50;
    std::vector<std::future<void>> futures;

    for (int i = 0; i < kTaskCount; ++i) {
        futures.push_back(pool.enqueue([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    EXPECT_EQ(counter.load(), kTaskCount);
}

TEST(ThreadPoolTest, GracefulShutdown) {
    auto pool = std::make_unique<bot::ThreadPool>(2);
    std::atomic<bool> executed{false};

    auto future = pool->enqueue([&executed]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        executed = true;
    });

    pool->shutdown();
    future.get();
    EXPECT_TRUE(executed.load());
}

// ============================================================================
// 3. Lazy Poster Caching Tests (file_id Caching and Fallback)
// ============================================================================

namespace {

class MockCacheMessenger : public bot::IBotMessenger {
public:
    struct PhotoRequest {
        int64_t chatId{0};
        std::string photoUrlOrFileId;
        std::string caption;
    };

    bool photoSuccess{true};
    std::string returnedFileId{"telegram_cached_file_id_999"};
    std::vector<PhotoRequest> photoRequests;
    std::vector<std::string> sentTexts;

    void sendMessage(int64_t, const std::string& text) override {
        sentTexts.push_back(text);
    }

    void sendMessageWithKeyboard(int64_t, const std::string& text, const bot::InlineKeyboard&) override {
        sentTexts.push_back(text);
    }

    bool sendPhotoWithKeyboard(
        int64_t chatId,
        const std::string& photoUrlOrFileId,
        const std::string& caption,
        const bot::InlineKeyboard&,
        std::string* fileIdOut = nullptr) override {
        photoRequests.push_back({chatId, photoUrlOrFileId, caption});

        if (!photoSuccess) {
            return false;
        }

        if (fileIdOut && !returnedFileId.empty()) {
            *fileIdOut = returnedFileId;
        }
        return true;
    }

    void answerCallbackQuery(const std::string&, const std::string& = "") override {}
};

} // namespace

TEST(PosterCachingTest, InstantDeliveryWhenFileIdCached) {
    MockCacheMessenger messenger;
    core::Movie movie;
    movie.id = 1;
    movie.title = "Cached Movie";
    movie.posterUrl = "https://example.com/poster.jpg";
    movie.posterFileId = "existing_file_id_123";

    bot::InlineKeyboard kb;
    bot::MovieCardFormatter::sendMovieCard(messenger, 1001, movie, kb);

    ASSERT_EQ(messenger.photoRequests.size(), 1u);
    EXPECT_EQ(messenger.photoRequests[0].chatId, 1001);
    // Sent via fast file_id instead of slow URL
    EXPECT_EQ(messenger.photoRequests[0].photoUrlOrFileId, "existing_file_id_123");
    EXPECT_TRUE(messenger.sentTexts.empty());
}

TEST(PosterCachingTest, LazyCachingViaStorageChat) {
    MockCacheMessenger messenger;
    messenger.returnedFileId = "newly_generated_file_id_777";

    core::Movie movie;
    movie.id = 10;
    movie.title = "Storage Movie";
    movie.posterUrl = "https://example.com/fresh_poster.jpg";
    movie.posterFileId = "";

    int64_t storageChatId = -100123456789;
    int64_t userChatId = 555;
    bool callbackInvoked = false;
    std::string savedFileId;

    bot::InlineKeyboard kb;
    bot::MovieCardFormatter::sendMovieCard(
        messenger,
        userChatId,
        movie,
        kb,
        storageChatId,
        [&](int64_t movieId, const std::string& fileId) {
            callbackInvoked = true;
            savedFileId = fileId;
            EXPECT_EQ(movieId, 10);
        });

    // 1st photo sent to storageChatId by URL
    // 2nd photo sent to userChatId by newly captured file_id
    ASSERT_EQ(messenger.photoRequests.size(), 2u);
    EXPECT_EQ(messenger.photoRequests[0].chatId, storageChatId);
    EXPECT_EQ(messenger.photoRequests[0].photoUrlOrFileId, "https://example.com/fresh_poster.jpg");

    EXPECT_EQ(messenger.photoRequests[1].chatId, userChatId);
    EXPECT_EQ(messenger.photoRequests[1].photoUrlOrFileId, "newly_generated_file_id_777");

    // Verified callback and local movie object caching
    EXPECT_TRUE(callbackInvoked);
    EXPECT_EQ(savedFileId, "newly_generated_file_id_777");
    EXPECT_EQ(movie.posterFileId, "newly_generated_file_id_777");
}

TEST(PosterCachingTest, DirectSendWhenNoStorageChatCapturesFileId) {
    MockCacheMessenger messenger;
    messenger.returnedFileId = "direct_file_id_888";

    core::Movie movie;
    movie.id = 20;
    movie.title = "Direct Movie";
    movie.posterUrl = "https://example.com/direct.jpg";

    int64_t userChatId = 777;
    std::string savedFileId;

    bot::InlineKeyboard kb;
    bot::MovieCardFormatter::sendMovieCard(
        messenger,
        userChatId,
        movie,
        kb,
        0, // No storage chat
        [&](int64_t, const std::string& fileId) {
            savedFileId = fileId;
        });

    ASSERT_EQ(messenger.photoRequests.size(), 1u);
    EXPECT_EQ(messenger.photoRequests[0].chatId, userChatId);
    EXPECT_EQ(messenger.photoRequests[0].photoUrlOrFileId, "https://example.com/direct.jpg");

    EXPECT_EQ(savedFileId, "direct_file_id_888");
    EXPECT_EQ(movie.posterFileId, "direct_file_id_888");
}

TEST(PosterCachingTest, FallbackToTextMessageWhenPhotoFails) {
    MockCacheMessenger messenger;
    messenger.photoSuccess = false; // Telegram photo upload failed

    core::Movie movie;
    movie.id = 30;
    movie.title = "Broken Poster Movie";
    movie.posterUrl = "https://example.com/broken.jpg";

    bot::InlineKeyboard kb;
    bot::MovieCardFormatter::sendMovieCard(messenger, 999, movie, kb);

    // Fallback to sendMessageWithKeyboard
    EXPECT_FALSE(messenger.sentTexts.empty());
    EXPECT_NE(messenger.sentTexts[0].find("Broken Poster Movie"), std::string::npos);
}

// ============================================================================
// 4. SQLite Repository poster_file_id Persistence Tests
// ============================================================================

TEST(RepositoryPerformanceTest, SQLitePosterFileIdPersistenceAndGenreMask) {
    repository::DatabaseManager dbManager;
    ASSERT_TRUE(dbManager.open(":memory:"));
    ASSERT_TRUE(dbManager.initializeSchema());

    auto movieRepo = std::make_shared<repository::SQLiteMovieRepository>(dbManager.getHandle());

    core::Movie movie;
    movie.id = 100;
    movie.title = "Fast & Furious";
    movie.year = 2001;
    movie.genres = {"Action", "Crime", "Thriller"};
    movie.director = "Rob Cohen";
    movie.description = "Street racer Dominic Toretto falls under the suspicion of LAPD.";
    movie.ratingAvg = 6.8;
    movie.posterUrl = "https://example.com/fast.jpg";
    movie.posterFileId = "";

    ASSERT_TRUE(movieRepo->addMovie(movie));

    // Initially poster_file_id should be empty, but genreMask populated
    auto retrieved = movieRepo->findById(100);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->posterFileId, "");
    EXPECT_NE(retrieved->genreMask, 0u);
    EXPECT_TRUE(retrieved->genreMask & static_cast<uint32_t>(core::GenreBit::Action));

    // Update poster_file_id lazily
    EXPECT_TRUE(movieRepo->updatePosterFileId(100, "telegram_file_id_fast_100"));

    // Verify it persists and is retrieved properly
    auto afterUpdate = movieRepo->findById(100);
    ASSERT_TRUE(afterUpdate.has_value());
    EXPECT_EQ(afterUpdate->posterFileId, "telegram_file_id_fast_100");
    EXPECT_EQ(afterUpdate->title, "Fast & Furious");
    EXPECT_EQ(afterUpdate->genreMask, retrieved->genreMask);
}
