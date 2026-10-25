#include <gtest/gtest.h>

#include "bot/BotStateMachine.h"
#include "bot/IBotMessenger.h"
#include "bot/KeyboardFactory.h"
#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"
#include "recommendation/RecommendationService.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @file test_bot_integration.cpp
 * @brief Сквозные интеграционные тесты диалогового бота с базой данных SQLite и сервисом рекомендаций (Задача 2.6).
 */

namespace {

/**
 * @class IntegrationSpyMessenger
 * @brief Шпион отправки сообщений Telegram для фиксации ответов FSM и разметки кнопок.
 */
class IntegrationSpyMessenger : public bot::IBotMessenger {
public:
    struct MessageRecord {
        int64_t chatId{0};
        std::string text;
        bot::InlineKeyboard keyboard;
    };

    void sendMessage(int64_t chatId, const std::string& text) override {
        messages.push_back({chatId, text, {}});
    }

    void sendMessageWithKeyboard(int64_t chatId, const std::string& text, const bot::InlineKeyboard& keyboard) override {
        messages.push_back({chatId, text, keyboard});
    }

    void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") override {
        answeredCallbacks.push_back({callbackId, text});
    }

    struct CallbackRecord {
        std::string callbackId;
        std::string text;
    };

    std::vector<MessageRecord> messages;
    std::vector<CallbackRecord> answeredCallbacks;
};

/**
 * @class BotIntegrationTest
 * @brief Тестовый стенд с реальной in-memory SQLite БД и датасетом из 60 фильмов.
 */
class BotIntegrationTest : public ::testing::Test {
protected:
    repository::DatabaseManager dbManager;
    std::shared_ptr<repository::SQLiteMovieRepository> movieRepo;
    std::shared_ptr<repository::SQLiteUserRepository> userRepo;
    std::shared_ptr<recommendation::RecommendationService> recService;
    std::shared_ptr<IntegrationSpyMessenger> spyMessenger;
    std::unique_ptr<bot::BotStateMachine> fsm;

    const int64_t testUserId = 777001;
    const int64_t testChatId = 999001;
    const std::string testUsername = "nolan_enthusiast";

    void SetUp() override {
        ASSERT_TRUE(dbManager.open(":memory:"));
        ASSERT_TRUE(dbManager.initializeSchema());
        ASSERT_TRUE(dbManager.seedMoviesFromJson("data/initial_movies.json"));

        movieRepo = std::make_shared<repository::SQLiteMovieRepository>(dbManager.getHandle());
        userRepo = std::make_shared<repository::SQLiteUserRepository>(dbManager.getHandle());
        recService = std::make_shared<recommendation::RecommendationService>(movieRepo, userRepo);

        spyMessenger = std::make_shared<IntegrationSpyMessenger>();
        fsm = std::make_unique<bot::BotStateMachine>(spyMessenger, recService, userRepo);
    }

    void TearDown() override {
        fsm.reset();
        recService.reset();
        movieRepo.reset();
        userRepo.reset();
        dbManager.close();
    }
};

} // namespace

/**
 * @test Сквозной тест онбординга: /start -> выбор жанра Sci-Fi -> сохранение в SQLite -> показ фильма по Cold-Start.
 */
TEST_F(BotIntegrationTest, FullOnboardingAndColdStartWithSQLite) {
    // 1. Пользователь отправляет команду /start
    fsm->processMessage({testChatId, testUserId, testUsername, "/start"});
    auto& sessionAfterStart = fsm->getSession(testUserId);
    EXPECT_EQ(sessionAfterStart.state, bot::BotState::SelectingGenres);

    // 2. Пользователь выбирает жанр Sci-Fi
    fsm->processCallback({"cb_toggle_1", testChatId, testUserId, 0, "genre:toggle:Sci-Fi"});

    // 3. Подтверждает выбор жанров
    fsm->processCallback({"cb_confirm_1", testChatId, testUserId, 0, "genre:confirm"});

    // 4. Проверяем автосохранение пользователя в таблице users базы SQLite
    auto dbUser = userRepo->getById(testUserId);
    ASSERT_TRUE(dbUser.has_value());
    EXPECT_EQ(dbUser->telegramId, testUserId);
    EXPECT_EQ(dbUser->username, testUsername);

    // 5. Проверяем сохранение любимого жанра Sci-Fi в таблице favorite_genres SQLite
    auto favGenres = recService->getFavoriteGenres(testUserId);
    ASSERT_FALSE(favGenres.empty());
    EXPECT_EQ(favGenres[0], "Sci-Fi");

    // 6. Проверяем переход в BrowsingRecommendations и карточку фильма из БД
    auto& session = fsm->getSession(testUserId);
    EXPECT_EQ(session.state, bot::BotState::BrowsingRecommendations);
    ASSERT_TRUE(session.currentMovie.has_value());
    EXPECT_EQ(session.currentMovie->title, "Inception");
    EXPECT_EQ(session.currentMovie->year, 2010);
    EXPECT_EQ(session.currentMovie->director, "Christopher Nolan");

    // 7. Проверяем, что текущая стратегия определена как Cold-Start / Popularity
    EXPECT_EQ(recService->resolveStrategyName(testUserId), "Cold-Start / Popularity");

    // 8. Проверяем, что карточка с фильмом была отправлена ботом в чат
    ASSERT_FALSE(spyMessenger->messages.empty());
    EXPECT_NE(spyMessenger->messages.back().text.find("Inception"), std::string::npos);
}

/**
 * @test Проверяет сохранение выставленной оценки в таблице ratings SQLite и авто-переключение стратегии на Content-Based.
 */
TEST_F(BotIntegrationTest, RatingPersistsToDatabaseAndSwitchesStrategyToContentBased) {
    // 1. Проходим онбординг
    fsm->processMessage({testChatId, testUserId, testUsername, "/start"});
    fsm->processCallback({"cb_scifi", testChatId, testUserId, 0, "genre:toggle:Sci-Fi"});
    fsm->processCallback({"cb_done", testChatId, testUserId, 0, "genre:confirm"});

    auto& session = fsm->getSession(testUserId);
    ASSERT_TRUE(session.currentMovie.has_value());
    int64_t firstMovieId = session.currentMovie->id;
    EXPECT_EQ(firstMovieId, 5); // Inception

    // 2. Пользователь нажимает «👍 Понравился» (rec:like -> 10/10)
    fsm->processCallback({"cb_like", testChatId, testUserId, 0, "rec:like"});

    // 3. Проверяем, что оценка 10/10 реально записалась в SQLite таблицу ratings
    auto ratings = recService->getUserRatings(testUserId);
    ASSERT_EQ(ratings.size(), 1);
    EXPECT_EQ(ratings[0].movieId, firstMovieId);
    EXPECT_EQ(ratings[0].score, 10);

    // 4. Проверяем, что стратегия алгоритма автоматически переключилась на Content-Based Filtering
    std::string currentStrategy = recService->resolveStrategyName(testUserId);
    EXPECT_EQ(currentStrategy, "Content-Based Filtering");

    // 5. Проверяем, что следующий выданный фильм не совпадает с уже оцененным Inception
    ASSERT_TRUE(session.currentMovie.has_value());
    EXPECT_NE(session.currentMovie->id, firstMovieId);
}

/**
 * @test Проверяет, что кнопка «Следующий» (rec:next) не повторяет один и тот же фильм при пропуске.
 */
TEST_F(BotIntegrationTest, SkipButtonDoesNotRepeatSameMovie) {
    // 1. Онбординг с жанром Sci-Fi
    fsm->processMessage({testChatId, testUserId, testUsername, "/start"});
    fsm->processCallback({"cb_1", testChatId, testUserId, 0, "genre:toggle:Sci-Fi"});
    fsm->processCallback({"cb_2", testChatId, testUserId, 0, "genre:confirm"});

    auto& session = fsm->getSession(testUserId);
    ASSERT_TRUE(session.currentMovie.has_value());
    int64_t firstMovieId = session.currentMovie->id;

    // 2. Нажимаем кнопку «Следующий» без выставления оценки
    fsm->processCallback({"cb_skip", testChatId, testUserId, 0, "rec:next"});

    // 3. Проверяем, что фильм зафиксирован как пропущенный и предложен другой фильм
    EXPECT_TRUE(session.isSkipped(firstMovieId));
    ASSERT_TRUE(session.currentMovie.has_value());
    EXPECT_NE(session.currentMovie->id, firstMovieId);

    // 4. Оценок в БД быть не должно
    EXPECT_TRUE(recService->getUserRatings(testUserId).empty());
}

/**
 * @test Проверяет восстановление сессии и истории оценок из базы данных при создании нового экземпляра FSM.
 */
TEST_F(BotIntegrationTest, HistoryAndSessionRestoreFromDatabase) {
    // 1. В первом экземпляре FSM выставляем оценку фильму #5
    fsm->processMessage({testChatId, testUserId, testUsername, "/start"});
    fsm->processCallback({"cb_1", testChatId, testUserId, 0, "genre:toggle:Sci-Fi"});
    fsm->processCallback({"cb_2", testChatId, testUserId, 0, "genre:confirm"});
    fsm->processCallback({"cb_like", testChatId, testUserId, 0, "rec:like"});

    // 2. Создаем совершенно новый экземпляр BotStateMachine поверх той же БД
    auto newSpy = std::make_shared<IntegrationSpyMessenger>();
    bot::BotStateMachine freshFsm(newSpy, recService, userRepo);

    // 3. Пользователь отправляет команду /history
    freshFsm.processMessage({testChatId, testUserId, testUsername, "/history"});

    // 4. Проверяем, что бот выгрузил историю из SQLite: фильм Inception, оценку 10/10 и активную стратегию
    ASSERT_FALSE(newSpy->messages.empty());
    const std::string& historyResponse = newSpy->messages.back().text;
    EXPECT_NE(historyResponse.find("Inception"), std::string::npos);
    EXPECT_NE(historyResponse.find("10/10"), std::string::npos);
    EXPECT_NE(historyResponse.find("Content-Based Filtering"), std::string::npos);

    // 5. Проверяем, что сессия восстановила оценки
    auto& freshSession = freshFsm.getSession(testUserId);
    EXPECT_EQ(freshSession.movieRatings.size(), 1);
    EXPECT_EQ(freshSession.movieRatings[5], 10);
}

/**
 * @test Проверяет, что команда /similar возвращает релевантные фильмы того же режиссера и жанра из датасета.
 */
TEST_F(BotIntegrationTest, SimilarMoviesCommandReturnsNolanMatches) {
    // 1. Запускаем просмотр фильма Inception
    fsm->processMessage({testChatId, testUserId, testUsername, "/start"});
    fsm->processCallback({"cb_1", testChatId, testUserId, 0, "genre:toggle:Sci-Fi"});
    fsm->processCallback({"cb_2", testChatId, testUserId, 0, "genre:confirm"});

    // 2. Отправляем команду /similar
    fsm->processMessage({testChatId, testUserId, testUsername, "/similar"});

    // 3. Проверяем, что бот прислал список похожих фильмов
    ASSERT_FALSE(spyMessenger->messages.empty());
    const std::string& similarResponse = spyMessenger->messages.back().text;
    EXPECT_NE(similarResponse.find("Фильмы, похожие на «Inception»"), std::string::npos);

    // В датасете сходство жанров (Action + Adventure + Sci-Fi) совпадают с вызовом getSimilarMovies
    auto expectedSimilar = recService->getSimilarMovies(5, 3);
    ASSERT_FALSE(expectedSimilar.empty());
    EXPECT_NE(similarResponse.find(expectedSimilar[0].title), std::string::npos);

    // 4. Проверяем обработку callback-запроса "rec:similar"
    fsm->processCallback({"cb_sim_btn", testChatId, testUserId, 0, "rec:similar"});
    EXPECT_NE(spyMessenger->messages.back().text.find("Фильмы, похожие на «Inception»"), std::string::npos);
}
