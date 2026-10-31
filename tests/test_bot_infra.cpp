#include <gtest/gtest.h>
#include "bot/IBotMessenger.h"
#include "bot/BotEventRouter.h"
#include "bot/UserSession.h"
#include "bot/BotConstants.h"

#include <vector>
#include <string>
#include <memory>

/**
 * @file test_bot_infra.cpp
 * @brief Модульные тесты для проверки маршрутизации событий, подтверждения callback-запросов и слоя мессенджера.
 */

namespace {

/**
 * @class MockBotMessenger
 * @brief Тестовый шпион (mock spy), реализующий `bot::IBotMessenger` для проверки отправки сообщений в памяти.
 */
class MockBotMessenger : public bot::IBotMessenger {
public:
    /// Структура записи отправленного текстового сообщения.
    struct SentMessage {
        int64_t chatId{0};
        std::string text;
    };

    /// Структура записи подтвержденного callback-запроса.
    struct AnsweredCallback {
        std::string callbackId;
        std::string text;
    };

    /**
     * @brief Фиксирует вызов отправки текстового сообщения.
     * @param chatId Идентификатор чата получателя.
     * @param text Текст сообщения.
     */
    void sendMessage(int64_t chatId, const std::string& text) override {
        sentMessages.push_back({chatId, text});
    }

    /**
     * @brief Фиксирует вызов подтверждения callback-запроса.
     * @param callbackId Идентификатор callback-запроса.
     * @param text Текст уведомления.
     */
    void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") override {
        answeredCallbacks.push_back({callbackId, text});
    }

    /// Список всех сообщений, отправленных через данный mock-объект.
    std::vector<SentMessage> sentMessages;

    /// Список всех подтверждений callback-запросов, зафиксированных данным mock-объектом.
    std::vector<AnsweredCallback> answeredCallbacks;
};

} // namespace

/**
 * @test Проверяет, что команда онбординга '/start' отправляет приветственное сообщение в чат автора.
 */
TEST(BotEventRouterTest, HandlesStartCommand) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotEventRouter router(mock);

    bot::IncomingMessage msg{123, 456, "stas", "/start"};
    router.onTextMessage(msg);

    ASSERT_EQ(mock->sentMessages.size(), 1);
    EXPECT_EQ(mock->sentMessages[0].chatId, 123);
    EXPECT_EQ(mock->sentMessages[0].text, "Welcome to Movie Recommendation Bot!");
}

/**
 * @test Проверяет, что обычное текстовое сообщение корректно передается в зарегистрированный обработчик.
 */
TEST(BotEventRouterTest, HandlesPlainTextMessage) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotEventRouter router(mock);

    bool customHandlerCalled = false;
    bot::IncomingMessage capturedMsg;

    router.setMessageHandler([&](const bot::IncomingMessage& msg) {
        customHandlerCalled = true;
        capturedMsg = msg;
    });

    bot::IncomingMessage msg{123, 456, "stas", "Hello bot"};
    router.onTextMessage(msg);

    EXPECT_TRUE(customHandlerCalled);
    EXPECT_EQ(capturedMsg.text, "Hello bot");
    EXPECT_EQ(capturedMsg.chatId, 123);
    EXPECT_EQ(capturedMsg.userId, 456);
}

/**
 * @test Проверяет, что при получении callback-запроса всегда отправляется подтверждение в Telegram API
 * и полезная нагрузка (payload) передается в зарегистрированный обработчик.
 */
TEST(BotEventRouterTest, HandlesCallbackQueryAndAcknowledgesIt) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotEventRouter router(mock);

    bool callbackHandlerCalled = false;
    bot::IncomingCallback capturedCallback;

    router.setCallbackHandler([&](const bot::IncomingCallback& cb) {
        callbackHandlerCalled = true;
        capturedCallback = cb;
    });

    bot::IncomingCallback cb{"cb_1", 123, 456, 10, "genre:sci-fi"};
    router.onCallbackQuery(cb);

    // Проверка снятия индикатора ожидания в клиенте Telegram
    ASSERT_EQ(mock->answeredCallbacks.size(), 1);
    EXPECT_EQ(mock->answeredCallbacks[0].callbackId, "cb_1");

    // Проверка передачи данных кнопки в обработчик
    EXPECT_TRUE(callbackHandlerCalled);
    EXPECT_EQ(capturedCallback.data, "genre:sci-fi");
    EXPECT_EQ(capturedCallback.chatId, 123);
    EXPECT_EQ(capturedCallback.userId, 456);
}

/**
 * @test Проверяет, что пустые входящие сообщения игнорируются и не вызывают отправки ответов.
 */
TEST(BotEventRouterTest, IgnoresEmptyMessage) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotEventRouter router(mock);

    bool handlerCalled = false;
    router.setMessageHandler([&](const bot::IncomingMessage&) {
        handlerCalled = true;
    });

    bot::IncomingMessage emptyMsg{123, 456, "stas", ""};
    router.onTextMessage(emptyMsg);

    EXPECT_FALSE(handlerCalled);
    EXPECT_TRUE(mock->sentMessages.empty());
}

/**
 * @test Проверяет, что быстрые повторные клики от одного пользователя подавляются (debounce).
 */
TEST(BotEventRouterTest, DebouncesRapidCallbacksPerUser) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotEventRouter router(mock);

    int callbackCount = 0;
    router.setCallbackHandler([&](const bot::IncomingCallback&) {
        ++callbackCount;
    });

    bot::IncomingCallback cb1{"cb_1", 123, 456, 10, "rec:like"};
    bot::IncomingCallback cb2{"cb_2", 123, 456, 10, "rec:like"};

    router.onCallbackQuery(cb1);
    router.onCallbackQuery(cb2); // дубликат в течение 400 мс

    // Оба запроса должны быть подтверждены в API (для прекращения спиннера в UI)
    EXPECT_EQ(mock->answeredCallbacks.size(), 2);
    // Но бизнес-обработчик должен вызваться только 1 раз!
    EXPECT_EQ(callbackCount, 1);
}

/**
 * @test Проверяет, что клики от разных пользователей не подавляют друг друга.
 */
TEST(BotEventRouterTest, DoesNotDebounceAcrossDifferentUsers) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotEventRouter router(mock);

    std::vector<int64_t> receivedUserIds;
    router.setCallbackHandler([&](const bot::IncomingCallback& cb) {
        receivedUserIds.push_back(cb.userId);
    });

    bot::IncomingCallback user1{"cb_1", 100, 1, 10, "rec:like"};
    bot::IncomingCallback user2{"cb_2", 200, 2, 10, "rec:like"};

    router.onCallbackQuery(user1);
    router.onCallbackQuery(user2);

    EXPECT_EQ(receivedUserIds.size(), 2);
    EXPECT_EQ(receivedUserIds[0], 1);
    EXPECT_EQ(receivedUserIds[1], 2);
}

/**
 * @test Проверяет методы инкапсуляции и принцип Tell, Don't Ask в структуре UserSession.
 */
TEST(UserSessionTest, EncapsulationMethodsAndTellDontAsk) {
    bot::UserSession session;
    EXPECT_FALSE(session.hasSelectedGenres());
    EXPECT_FALSE(session.hasRatings());
    EXPECT_FALSE(session.hasCurrentMovie());
    EXPECT_EQ(session.getRatingCount(), 0);

    // Тестирование жанров
    session.setSelectedGenres({"Drama", "Action"});
    EXPECT_TRUE(session.hasSelectedGenres());
    EXPECT_TRUE(session.hasGenre("Drama"));
    EXPECT_FALSE(session.hasGenre("Comedy"));
    EXPECT_EQ(session.getSelectedGenres().size(), 2);

    // Тестирование оценок
    session.recordRating(42, 9);
    EXPECT_TRUE(session.hasRatings());
    EXPECT_TRUE(session.hasRating(42));
    EXPECT_FALSE(session.hasRating(99));
    EXPECT_EQ(session.getRating(42), std::optional<int>(9));
    EXPECT_EQ(session.getRating(99), std::nullopt);
    EXPECT_EQ(session.getRatingCount(), 1);

    // Пропуск фильма
    session.markSkipped(100);
    EXPECT_TRUE(session.isSkipped(100));
    EXPECT_FALSE(session.isSkipped(101));

    // Выставление оценки ранее пропущенному фильму снимает статус пропуска
    session.recordRating(100, 8);
    EXPECT_FALSE(session.isSkipped(100));

    // Уведомление о последнем действии
    session.setLastActionNotice("Action notice text");
    EXPECT_EQ(session.getLastActionNotice(), "Action notice text");
    std::string consumed = session.consumeLastActionNotice();
    EXPECT_EQ(consumed, "Action notice text");
    EXPECT_TRUE(session.getLastActionNotice().empty());

    // Текущий фильм
    core::Movie m;
    m.id = 55;
    m.title = "Test Movie";
    session.setCurrentMovie(m);
    EXPECT_TRUE(session.hasCurrentMovie());
    EXPECT_EQ(session.getCurrentMovie()->id, 55);
    EXPECT_EQ(session.getCurrentMovieId(), std::optional<int64_t>(55));
    EXPECT_EQ(session.getCurrentMovieTitle(), "Test Movie");
    session.clearCurrentMovie();
    EXPECT_FALSE(session.hasCurrentMovie());
    EXPECT_EQ(session.getCurrentMovieId(), std::nullopt);
    EXPECT_EQ(session.getCurrentMovieTitle(), "");

    // Закладки («Буду смотреть»)
    EXPECT_FALSE(session.isBookmarked(42));
    EXPECT_FALSE(session.isBookmarked(0));
    EXPECT_FALSE(session.isBookmarked(-5));
    EXPECT_FALSE(session.toggleBookmark(0));
    EXPECT_FALSE(session.toggleBookmark(-10));
    EXPECT_TRUE(session.getBookmarkedMovieIds().empty());
    EXPECT_TRUE(session.toggleBookmark(42));
    EXPECT_TRUE(session.isBookmarked(42));
    EXPECT_EQ(session.getBookmarkedMovieIds().size(), 1);
    EXPECT_EQ(session.getBookmarkedMovieIds()[0], 42);

    // Сортировка множественных закладок
    EXPECT_TRUE(session.toggleBookmark(15));
    EXPECT_TRUE(session.toggleBookmark(99));
    auto sortedIds = session.getBookmarkedMovieIds();
    EXPECT_EQ(sortedIds.size(), 3);
    EXPECT_EQ(sortedIds[0], 15);
    EXPECT_EQ(sortedIds[1], 42);
    EXPECT_EQ(sortedIds[2], 99);
    EXPECT_FALSE(session.toggleBookmark(15));
    EXPECT_FALSE(session.toggleBookmark(99));

    EXPECT_FALSE(session.toggleBookmark(42));
    EXPECT_FALSE(session.isBookmarked(42));
    EXPECT_TRUE(session.getBookmarkedMovieIds().empty());

    // Полный сброс
    session.toggleBookmark(99);
    EXPECT_TRUE(session.isBookmarked(99));
    session.resetSessionData();
    EXPECT_FALSE(session.hasSelectedGenres());
    EXPECT_FALSE(session.hasRatings());
    EXPECT_FALSE(session.hasCurrentMovie());
    EXPECT_FALSE(session.isBookmarked(99));
    EXPECT_TRUE(session.getBookmarkedMovieIds().empty());
    EXPECT_EQ(session.getRatingCount(), 0);
}

/**
 * @test Проверяет значения централизованных именованных констант BotConstants.
 */
TEST(BotConstantsTest, VerifyDefaultValues) {
    EXPECT_EQ(bot::kRatingLike, 10);
    EXPECT_EQ(bot::kRatingDislike, 1);
    EXPECT_EQ(bot::kMinRating, 1);
    EXPECT_EQ(bot::kMaxRating, 10);
    EXPECT_EQ(bot::kPositiveRatingThreshold, 7);
    EXPECT_EQ(bot::kDefaultSimilarMoviesLimit, 3);
    EXPECT_EQ(bot::kRecommendationBatchSize, 20);
    EXPECT_EQ(bot::kDebounceTimeoutMs, 300);
    EXPECT_EQ(bot::kDefaultThreadPoolSize, 4);
    EXPECT_EQ(bot::kDefaultLongPollTimeoutSeconds, 10);
    EXPECT_EQ(bot::kDefaultLongPollLimit, 100);
}
