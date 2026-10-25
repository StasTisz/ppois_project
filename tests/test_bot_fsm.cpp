#include <gtest/gtest.h>
#include "bot/BotStateMachine.h"
#include "bot/IBotMessenger.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @file test_bot_fsm.cpp
 * @brief Модульные тесты для проверки диалогового конечного автомата (FSM) и переходов состояний.
 */

namespace {

/**
 * @class MockBotMessenger
 * @brief Тестовый шпион, фиксирующий отправленные сообщения и callback-ответы в памяти.
 */
class MockBotMessenger : public bot::IBotMessenger {
public:
    struct SentMessage {
        int64_t chatId{0};
        std::string text;
    };

    struct AnsweredCallback {
        std::string callbackId;
        std::string text;
    };

    void sendMessage(int64_t chatId, const std::string& text) override {
        sentMessages.push_back({chatId, text});
    }

    void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") override {
        answeredCallbacks.push_back({callbackId, text});
    }

    std::vector<SentMessage> sentMessages;
    std::vector<AnsweredCallback> answeredCallbacks;
};

} // namespace

/**
 * @test Проверяет, что новый пользователь начинает работу в состоянии Idle.
 */
TEST(BotFsmTest, InitialStateIsIdle) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotStateMachine fsm(mock);

    const auto& session = fsm.getSession(1001, 12345, "alice");
    EXPECT_EQ(session.state, bot::BotState::Idle);
    EXPECT_EQ(session.userId, 1001);
    EXPECT_EQ(session.chatId, 12345);
    EXPECT_EQ(session.username, "alice");
}

/**
 * @test Проверяет, что команда /start переводит пользователя в состояние SelectingGenres.
 */
TEST(BotFsmTest, StartCommandTransitionsToGenreSelection) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotStateMachine fsm(mock);

    bot::IncomingMessage msg{12345, 1001, "alice", "/start"};
    fsm.processMessage(msg);

    const auto& session = fsm.getSession(1001);
    EXPECT_EQ(session.state, bot::BotState::SelectingGenres);
    EXPECT_FALSE(mock->sentMessages.empty());
}

/**
 * @test Проверяет цикл переключения жанров и подтверждения выбора.
 */
TEST(BotFsmTest, GenreToggleAndConfirmFlow) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotStateMachine fsm(mock);

    // Запуск онбординга
    fsm.processMessage({12345, 1001, "alice", "/start"});
    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::SelectingGenres);

    // Попытка подтвердить пустой список жанров -> пользователь остается в SelectingGenres
    fsm.processCallback({"cb_1", 12345, 1001, 10, "genre:confirm"});
    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::SelectingGenres);

    // Выбор жанров Sci-Fi и Drama
    fsm.processCallback({"cb_2", 12345, 1001, 10, "genre:toggle:Sci-Fi"});
    fsm.processCallback({"cb_3", 12345, 1001, 10, "genre:toggle:Drama"});

    EXPECT_TRUE(fsm.getSession(1001).hasGenre("Sci-Fi"));
    EXPECT_TRUE(fsm.getSession(1001).hasGenre("Drama"));
    EXPECT_EQ(fsm.getSession(1001).selectedGenres.size(), 2);

    // Повторное нажатие Drama -> удаляет Drama
    fsm.processCallback({"cb_4", 12345, 1001, 10, "genre:toggle:Drama"});
    EXPECT_FALSE(fsm.getSession(1001).hasGenre("Drama"));
    EXPECT_TRUE(fsm.getSession(1001).hasGenre("Sci-Fi"));
    EXPECT_EQ(fsm.getSession(1001).selectedGenres.size(), 1);

    // Подтверждение с выбранным жанром Sci-Fi -> переход в BrowsingRecommendations
    fsm.processCallback({"cb_5", 12345, 1001, 10, "genre:confirm"});
    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::BrowsingRecommendations);
    EXPECT_TRUE(fsm.getSession(1001).currentMovie.has_value());
    EXPECT_EQ(fsm.getSession(1001).currentMovie->id, 1);
}

/**
 * @test Проверяет реакции Like, Dislike и пропуск фильма при просмотре рекомендаций.
 */
TEST(BotFsmTest, BrowsingReactionsLikeDislikeNext) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotStateMachine fsm(mock);

    fsm.processMessage({12345, 1001, "alice", "/start"});
    fsm.processCallback({"cb_1", 12345, 1001, 10, "genre:toggle:Sci-Fi"});
    fsm.processCallback({"cb_2", 12345, 1001, 10, "genre:confirm"});
    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::BrowsingRecommendations);

    ASSERT_TRUE(fsm.getSession(1001).currentMovie.has_value());
    int64_t firstMovieId = fsm.getSession(1001).currentMovie->id;

    // Реакция "Лайк" -> оценка 10 и переход к следующему фильму
    fsm.processCallback({"cb_3", 12345, 1001, 10, "rec:like"});
    EXPECT_EQ(fsm.getSession(1001).movieRatings.at(firstMovieId), 10);
    EXPECT_TRUE(fsm.getSession(1001).currentMovie.has_value());
    int64_t secondMovieId = fsm.getSession(1001).currentMovie->id;
    EXPECT_NE(firstMovieId, secondMovieId);

    // Реакция "Дизлайк" на второй фильм -> оценка 1 и переход к следующему фильму
    fsm.processCallback({"cb_4", 12345, 1001, 10, "rec:dislike"});
    EXPECT_EQ(fsm.getSession(1001).movieRatings.at(secondMovieId), 1);
}

/**
 * @test Проверяет валидацию оценки, сохранение значения и возврат в режим рекомендаций.
 */
TEST(BotFsmTest, RatingStateValidationAndTransition) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotStateMachine fsm(mock);

    fsm.processMessage({12345, 1001, "alice", "/start"});
    fsm.processCallback({"cb_1", 12345, 1001, 10, "genre:toggle:Action"});
    fsm.processCallback({"cb_2", 12345, 1001, 10, "genre:confirm"});
    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::BrowsingRecommendations);

    ASSERT_TRUE(fsm.getSession(1001).currentMovie.has_value());
    int64_t currentMovieId = fsm.getSession(1001).currentMovie->id;

    // Переход в режим оценки по кнопке
    fsm.processCallback({"cb_3", 12345, 1001, 10, "rec:rate"});
    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::RatingMovie);

    // Некорректная оценка "15" -> остается в RatingMovie
    fsm.processMessage({12345, 1001, "alice", "15"});
    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::RatingMovie);

    // Некорректный текст "abc" -> остается в RatingMovie
    fsm.processMessage({12345, 1001, "alice", "abc"});
    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::RatingMovie);

    // Корректная оценка "8" -> сохраняется и переводит обратно в BrowsingRecommendations
    fsm.processMessage({12345, 1001, "alice", "8"});
    EXPECT_EQ(fsm.getSession(1001).movieRatings.at(currentMovieId), 8);
    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::BrowsingRecommendations);
}

/**
 * @test Проверяет изоляцию состояний и данных между несколькими пользователями.
 */
TEST(BotFsmTest, MultiUserIsolation) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotStateMachine fsm(mock);

    // Пользователь 101 начинает выбор жанров
    fsm.processMessage({1001, 101, "user101", "/start"});
    fsm.processCallback({"cb_1", 1001, 101, 10, "genre:toggle:Comedy"});

    // Пользователь 202 выбирает Sci-Fi и переходит в просмотр
    fsm.processMessage({2002, 202, "user202", "/start"});
    fsm.processCallback({"cb_2", 2002, 202, 10, "genre:toggle:Sci-Fi"});
    fsm.processCallback({"cb_3", 2002, 202, 10, "genre:confirm"});

    const auto& session1 = fsm.getSession(101);
    const auto& session2 = fsm.getSession(202);

    EXPECT_EQ(session1.state, bot::BotState::SelectingGenres);
    EXPECT_EQ(session2.state, bot::BotState::BrowsingRecommendations);

    EXPECT_TRUE(session1.hasGenre("Comedy"));
    EXPECT_FALSE(session1.hasGenre("Sci-Fi"));

    EXPECT_TRUE(session2.hasGenre("Sci-Fi"));
    EXPECT_FALSE(session2.hasGenre("Comedy"));
}
