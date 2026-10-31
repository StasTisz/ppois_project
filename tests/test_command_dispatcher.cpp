#include <gtest/gtest.h>

#include "bot/BotStateMachine.h"
#include "bot/CommandDispatcher.h"
#include "bot/IBotMessenger.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @file test_command_dispatcher.cpp
 * @brief Модульные тесты для проверки диспетчера команд CommandDispatcher.
 */

namespace {

/**
 * @class DispatcherSpyMessenger
 * @brief Тестовый шпион мессенджера, сохраняющий текст и прикрепленные inline-клавиатуры.
 */
class DispatcherSpyMessenger : public bot::IBotMessenger {
public:
    struct SentEntry {
        int64_t chatId{0};
        std::string text;
        bot::InlineKeyboard keyboard;
    };

    void sendMessage(int64_t chatId, const std::string& text) override {
        entries.push_back({chatId, text, {}});
    }

    void sendMessageWithKeyboard(int64_t chatId, const std::string& text, const bot::InlineKeyboard& keyboard) override {
        entries.push_back({chatId, text, keyboard});
    }

    void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") override {
        (void)callbackId;
        (void)text;
    }

    std::vector<SentEntry> entries;
};

} // namespace

/**
 * @test Проверяет корректность разбора имени команды, аргументов и удаления упоминания бота (@botname).
 */
TEST(CommandDispatcherTest, CommandParsingAndBotMention) {
    EXPECT_EQ(bot::CommandDispatcher::extractCommand("/start"), "/start");
    EXPECT_EQ(bot::CommandDispatcher::extractCommand("/recommend@my_movie_bot"), "/recommend");
    EXPECT_EQ(bot::CommandDispatcher::extractCommand("/help now"), "/help");
    EXPECT_EQ(bot::CommandDispatcher::extractCommand("plain text"), "");
    EXPECT_EQ(bot::CommandDispatcher::extractCommand("/genres@bot_name param1 param2"), "/genres");
    EXPECT_EQ(bot::CommandDispatcher::extractCommand(""), "");
    EXPECT_EQ(bot::CommandDispatcher::extractCommand("/"), "/");
}

/**
 * @test Проверяет, что команда /start переводит пользователя в режим онбординга (SelectingGenres) и шлет клавиатуру жанров.
 */
TEST(CommandDispatcherTest, StartCommandInitiatesOnboarding) {
    auto spy = std::make_shared<DispatcherSpyMessenger>();
    bot::BotStateMachine fsm(spy);

    bot::IncomingMessage msg{12345, 1001, "alice", "/start"};
    fsm.processMessage(msg);

    const auto& session = fsm.getSession(1001);
    EXPECT_EQ(session.state, bot::BotState::SelectingGenres);
    ASSERT_FALSE(spy->entries.empty());

    bool foundGenreKeyboard = false;
    for (const auto& entry : spy->entries) {
        if (entry.keyboard.findByCallback("genre:confirm") != nullptr) {
            foundGenreKeyboard = true;
            break;
        }
    }
    EXPECT_TRUE(foundGenreKeyboard);
}

/**
 * @test Проверяет, что /recommend при отсутствии выбранных жанров перенаправляет в SelectingGenres.
 */
TEST(CommandDispatcherTest, RecommendWithoutGenresRedirectsToGenres) {
    auto spy = std::make_shared<DispatcherSpyMessenger>();
    bot::BotStateMachine fsm(spy);

    bot::IncomingMessage msg{12345, 1001, "alice", "/recommend"};
    fsm.processMessage(msg);

    const auto& session = fsm.getSession(1001);
    EXPECT_EQ(session.state, bot::BotState::SelectingGenres);
    EXPECT_TRUE(session.selectedGenres.empty());
}

/**
 * @test Проверяет, что /recommend при выбранных жанрах сразу начинает показ фильмов с кнопками реакций.
 */
TEST(CommandDispatcherTest, RecommendWithGenresStartsBrowsing) {
    auto spy = std::make_shared<DispatcherSpyMessenger>();
    bot::BotStateMachine fsm(spy);

    auto& session = fsm.getSession(1001, 12345, "alice");
    session.selectedGenres = {"Sci-Fi"};

    bot::IncomingMessage msg{12345, 1001, "alice", "/recommend"};
    fsm.processMessage(msg);

    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::BrowsingRecommendations);
    ASSERT_TRUE(fsm.getSession(1001).currentMovie.has_value());
    ASSERT_FALSE(spy->entries.empty());

    const auto& lastEntry = spy->entries.back();
    EXPECT_NE(lastEntry.keyboard.findByCallback("rec:like"), nullptr);
    EXPECT_NE(lastEntry.keyboard.findByCallback("rec:dislike"), nullptr);
    EXPECT_NE(lastEntry.keyboard.findByCallback("rec:next"), nullptr);
    EXPECT_NE(lastEntry.keyboard.findByCallback("rec:rate"), nullptr);
}

/**
 * @test Проверяет, что команда /genres открывает выбор жанров из любого состояния.
 */
TEST(CommandDispatcherTest, GenresCommandOpensGenreSelection) {
    auto spy = std::make_shared<DispatcherSpyMessenger>();
    bot::BotStateMachine fsm(spy);

    auto& session = fsm.getSession(1001, 12345, "alice");
    session.selectedGenres = {"Drama"};
    fsm.transitionTo(1001, bot::BotState::BrowsingRecommendations);
    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::BrowsingRecommendations);

    bot::IncomingMessage msg{12345, 1001, "alice", "/genres"};
    fsm.processMessage(msg);

    EXPECT_EQ(fsm.getSession(1001).state, bot::BotState::SelectingGenres);
}

/**
 * @test Проверяет вывод команды /history для нового пользователя и для пользователя с оцененными фильмами.
 */
TEST(CommandDispatcherTest, HistoryCommandEmptyAndPopulated) {
    auto spy = std::make_shared<DispatcherSpyMessenger>();
    bot::BotStateMachine fsm(spy);

    // 1. История нового пользователя
    fsm.processMessage({12345, 1001, "alice", "/history"});
    ASSERT_FALSE(spy->entries.empty());
    EXPECT_NE(spy->entries.back().text.find("Ваша история просмотров и оценок пока пуста."), std::string::npos);
    EXPECT_NE(spy->entries.back().keyboard.findByCallback("menu:recommend"), nullptr);

    // 2. История с оцененным фильмом #1 ("Начало (Inception)")
    auto& session = fsm.getSession(1001);
    session.movieRatings[1] = 9;

    fsm.processMessage({12345, 1001, "alice", "/history"});
    const auto& lastMsg = spy->entries.back();
    EXPECT_NE(lastMsg.text.find("Начало (Inception)"), std::string::npos);
    EXPECT_NE(lastMsg.text.find("9/10"), std::string::npos);
    EXPECT_NE(lastMsg.keyboard.findByCallback("menu:recommend"), nullptr);
}

/**
 * @test Проверяет, что команда /help выводит список всех команд и меню навигации.
 */
TEST(CommandDispatcherTest, HelpCommandOutputsCommandsList) {
    auto spy = std::make_shared<DispatcherSpyMessenger>();
    bot::BotStateMachine fsm(spy);

    fsm.processMessage({12345, 1001, "alice", "/help"});
    ASSERT_FALSE(spy->entries.empty());

    const auto& lastMsg = spy->entries.back();
    EXPECT_NE(lastMsg.text.find("/start"), std::string::npos);
    EXPECT_NE(lastMsg.text.find("/recommend"), std::string::npos);
    EXPECT_NE(lastMsg.text.find("/genres"), std::string::npos);
    EXPECT_NE(lastMsg.text.find("/history"), std::string::npos);
    EXPECT_NE(lastMsg.text.find("/help"), std::string::npos);

    EXPECT_FALSE(lastMsg.keyboard.empty());
    EXPECT_NE(lastMsg.keyboard.findByCallback("menu:recommend"), nullptr);
    EXPECT_NE(lastMsg.keyboard.findByCallback("menu:genres"), nullptr);
}

/**
 * @test Проверяет, что команда /stats выводит сводку метрик и прикрепляет главное меню.
 */
TEST(CommandDispatcherTest, StatsCommandOutputsMetricsReport) {
    auto spy = std::make_shared<DispatcherSpyMessenger>();
    bot::BotStateMachine fsm(spy);

    auto collector = std::make_shared<analytics::MetricsCollector>();
    collector->recordImpression(1001, 1);
    collector->recordLike(1001, 1);
    fsm.setMetricsCollector(collector);

    fsm.processMessage({12345, 1001, "alice", "/stats"});
    ASSERT_FALSE(spy->entries.empty());

    const auto& lastMsg = spy->entries.back();
    EXPECT_NE(lastMsg.text.find("Метрики рекомендательной системы"), std::string::npos);
    EXPECT_NE(lastMsg.text.find("CTR"), std::string::npos);
    EXPECT_NE(lastMsg.text.find("DAU"), std::string::npos);
    EXPECT_FALSE(lastMsg.keyboard.empty());
    EXPECT_NE(lastMsg.keyboard.findByCallback("menu:recommend"), nullptr);
}

/**
 * @test Проверяет команду /bookmarks и /watchlist при пустом списке закладок.
 */
TEST(CommandDispatcherTest, BookmarksCommandWhenEmpty) {
    auto spy = std::make_shared<DispatcherSpyMessenger>();
    bot::BotStateMachine fsm(spy);

    // 1. Команда /bookmarks
    fsm.processMessage({12345, 1001, "alice", "/bookmarks"});
    ASSERT_FALSE(spy->entries.empty());
    std::string emptyExpected = "🔖 Ваш список закладок «Буду смотреть» пока пуст.\n"
                                "Вы можете добавить фильм в закладки кнопкой «🔖 Буду смотреть» на карточке фильма.";
    EXPECT_EQ(spy->entries.back().text, emptyExpected);
    EXPECT_NE(spy->entries.back().keyboard.findByCallback("menu:recommend"), nullptr);

    // 2. Алиас /watchlist
    fsm.processMessage({12345, 1001, "alice", "/watchlist"});
    EXPECT_EQ(spy->entries.back().text, emptyExpected);
    EXPECT_NE(spy->entries.back().keyboard.findByCallback("menu:bookmarks"), nullptr);
}

/**
 * @test Проверяет форматирование списка закладок при наличии сохраненных фильмов.
 */
TEST(CommandDispatcherTest, BookmarksCommandWithSavedMovies) {
    auto spy = std::make_shared<DispatcherSpyMessenger>();
    bot::BotStateMachine fsm(spy);

    auto& session = fsm.getSession(1001, 12345, "alice");
    session.toggleBookmark(1);

    fsm.processMessage({12345, 1001, "alice", "/bookmarks"});
    ASSERT_FALSE(spy->entries.empty());

    const auto& lastMsg = spy->entries.back();
    EXPECT_NE(lastMsg.text.find("🔖 Ваш список закладок «Буду смотреть»:"), std::string::npos);
    EXPECT_NE(lastMsg.text.find("Начало (Inception)"), std::string::npos);
    EXPECT_NE(lastMsg.text.find("2010"), std::string::npos);
    EXPECT_NE(lastMsg.text.find("8.8/10"), std::string::npos);
    EXPECT_NE(lastMsg.text.find("Sci-Fi"), std::string::npos);
    EXPECT_FALSE(lastMsg.keyboard.empty());
    EXPECT_NE(lastMsg.keyboard.findByCallback("menu:recommend"), nullptr);
}

namespace {

class DummyBookmarkContext : public bot::IDialogStateContext {
public:
    bot::UserSession& getSession(int64_t, int64_t, const std::string&) override { return session_; }
    void transitionTo(int64_t, bot::BotState) override {}
    std::shared_ptr<bot::IBotMessenger> getMessenger() const override { return nullptr; }
    std::optional<core::Movie> fetchNextMovie(int64_t) override { return std::nullopt; }
    [[nodiscard]] std::optional<core::Movie> getMovieById(int64_t movieId) const override {
        if (movieId == 42) {
            return core::Movie{42, "Матрица", 1999, {"Sci-Fi", "Action"}, "Вачовски",
                "Описание", 8.7, "", "", "https://youtube.com/watch?v=matrix"};
        }
        return std::nullopt;
    }
private:
    bot::UserSession session_;
};

} // namespace

/**
 * @test Проверяет форматирование фильма в закладках со ссылкой на трейлер.
 */
TEST(CommandDispatcherTest, FormatBookmarksTextWithTrailer) {
    DummyBookmarkContext ctx;
    bot::UserSession session;
    session.toggleBookmark(42);

    std::string text = bot::CommandDispatcher::formatBookmarksText(session, ctx);
    EXPECT_NE(text.find("🔖 Ваш список закладок «Буду смотреть»:"), std::string::npos);
    EXPECT_NE(text.find("Матрица"), std::string::npos);
    EXPECT_NE(text.find("(1999)"), std::string::npos);
    EXPECT_NE(text.find("★ 8.7/10"), std::string::npos);
    EXPECT_NE(text.find("Sci-Fi, Action"), std::string::npos);
    EXPECT_NE(text.find("🎬 Трейлер: https://youtube.com/watch?v=matrix"), std::string::npos);
}

/**
 * @test Проверяет, что команда /help упоминает команду /bookmarks.
 */
TEST(CommandDispatcherTest, HelpCommandMentionsBookmarks) {
    auto spy = std::make_shared<DispatcherSpyMessenger>();
    bot::BotStateMachine fsm(spy);

    fsm.processMessage({12345, 1001, "alice", "/help"});
    ASSERT_FALSE(spy->entries.empty());

    const auto& lastMsg = spy->entries.back();
    EXPECT_NE(lastMsg.text.find("/bookmarks"), std::string::npos);
}

