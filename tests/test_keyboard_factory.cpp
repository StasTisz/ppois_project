#include <gtest/gtest.h>

#include "bot/BotStateMachine.h"
#include "bot/IBotMessenger.h"
#include "bot/KeyboardFactory.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @file test_keyboard_factory.cpp
 * @brief Модульные тесты для проверки генерации Inline-клавиатур фабрикой KeyboardFactory.
 */

namespace {

/**
 * @class KeyboardSpyMessenger
 * @brief Тестовый дублер мессенджера, сохраняющий текст и переданную inline-клавиатуру.
 */
class KeyboardSpyMessenger : public bot::IBotMessenger {
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
        (void)callbackId;
        (void)text;
    }

    std::vector<MessageRecord> messages;
};

} // namespace

/**
 * @test Проверяет, что выбранные жанры отмечаются галочкой, а невыбранные остаются без галочки.
 */
TEST(KeyboardFactoryTest, GenreSelectionCheckmarks) {
    std::vector<std::string> selected = {"Sci-Fi", "Drama"};
    auto kb = bot::KeyboardFactory::createGenreSelectionKeyboard(selected);

    EXPECT_FALSE(kb.empty());
    EXPECT_GT(kb.buttonCount(), 0);

    const auto* sciFiBtn = kb.findByCallback("genre:toggle:Sci-Fi");
    ASSERT_NE(sciFiBtn, nullptr);
    EXPECT_EQ(sciFiBtn->text, "✅ Sci-Fi");

    const auto* dramaBtn = kb.findByCallback("genre:toggle:Drama");
    ASSERT_NE(dramaBtn, nullptr);
    EXPECT_EQ(dramaBtn->text, "✅ Drama");

    const auto* actionBtn = kb.findByCallback("genre:toggle:Action");
    ASSERT_NE(actionBtn, nullptr);
    EXPECT_EQ(actionBtn->text, "Action");

    const auto* confirmBtn = kb.findByCallback("genre:confirm");
    ASSERT_NE(confirmBtn, nullptr);
    EXPECT_EQ(confirmBtn->text, "✔️ Готово");
    ASSERT_FALSE(kb.rows.empty());
    EXPECT_EQ(kb.rows.back().size(), 1);
    EXPECT_EQ(kb.rows.back()[0].callbackData, "genre:confirm");
}

/**
 * @test Проверяет разметку и callback-данные кнопок реакций под карточкой фильма.
 */
TEST(KeyboardFactoryTest, MovieReactionButtonsLayout) {
    auto kb = bot::KeyboardFactory::createMovieReactionKeyboard();

    EXPECT_FALSE(kb.empty());
    EXPECT_EQ(kb.buttonCount(), 5);

    const auto* likeBtn = kb.findByCallback("rec:like");
    ASSERT_NE(likeBtn, nullptr);
    EXPECT_EQ(likeBtn->text, "👍 Понравился");

    const auto* dislikeBtn = kb.findByCallback("rec:dislike");
    ASSERT_NE(dislikeBtn, nullptr);
    EXPECT_EQ(dislikeBtn->text, "👎 Не понравился");

    const auto* nextBtn = kb.findByCallback("rec:next");
    ASSERT_NE(nextBtn, nullptr);
    EXPECT_EQ(nextBtn->text, "⏭ Следующий");

    const auto* rateBtn = kb.findByCallback("rec:rate");
    ASSERT_NE(rateBtn, nullptr);
    EXPECT_EQ(rateBtn->text, "⭐ Оценить (1-10)");

    const auto* menuBtn = kb.findByCallback("rec:menu");
    ASSERT_NE(menuBtn, nullptr);
    EXPECT_EQ(menuBtn->text, "🏠 В меню");
}

/**
 * @test Проверяет наличие всех кнопок оценки от 1 до 10 и кнопки отмены.
 */
TEST(KeyboardFactoryTest, RatingButtons1To10) {
    auto kb = bot::KeyboardFactory::createRatingKeyboard();

    EXPECT_FALSE(kb.empty());
    EXPECT_EQ(kb.buttonCount(), 11);

    for (int i = 1; i <= 10; ++i) {
        std::string cb = "rate:" + std::to_string(i);
        const auto* btn = kb.findByCallback(cb);
        ASSERT_NE(btn, nullptr);
        EXPECT_EQ(btn->text, std::to_string(i));
    }

    const auto* cancelBtn = kb.findByCallback("rate:cancel");
    ASSERT_NE(cancelBtn, nullptr);
    EXPECT_EQ(cancelBtn->text, "🔙 Отмена");
}

/**
 * @test Проверяет разметку и callback-данные кнопок главного меню.
 */
TEST(KeyboardFactoryTest, MainMenuKeyboard) {
    auto kb = bot::KeyboardFactory::createMainMenuKeyboard();

    EXPECT_FALSE(kb.empty());
    EXPECT_EQ(kb.buttonCount(), 4);

    EXPECT_NE(kb.findByCallback("menu:recommend"), nullptr);
    EXPECT_NE(kb.findByCallback("menu:genres"), nullptr);
    EXPECT_NE(kb.findByCallback("menu:history"), nullptr);
    EXPECT_NE(kb.findByCallback("menu:help"), nullptr);
}

/**
 * @test Проверяет, что при входе в состояния FSM отправляются сообщения с прикрепленными клавиатурами.
 */
TEST(KeyboardFactoryTest, FsmStatesSendKeyboards) {
    auto spy = std::make_shared<KeyboardSpyMessenger>();
    bot::BotStateMachine fsm(spy);

    // 1. Состояние SelectingGenres
    fsm.transitionTo(1001, bot::BotState::SelectingGenres);
    ASSERT_FALSE(spy->messages.empty());
    EXPECT_FALSE(spy->messages.back().keyboard.empty());
    EXPECT_NE(spy->messages.back().keyboard.findByCallback("genre:confirm"), nullptr);

    // 2. Состояние BrowsingRecommendations
    fsm.getSession(1001).selectedGenres = {"Sci-Fi"};
    fsm.transitionTo(1001, bot::BotState::BrowsingRecommendations);
    EXPECT_FALSE(spy->messages.back().keyboard.empty());
    EXPECT_NE(spy->messages.back().keyboard.findByCallback("rec:like"), nullptr);

    // 3. Состояние RatingMovie
    fsm.transitionTo(1001, bot::BotState::RatingMovie);
    EXPECT_FALSE(spy->messages.back().keyboard.empty());
    EXPECT_NE(spy->messages.back().keyboard.findByCallback("rate:10"), nullptr);
}

/**
 * @test Проверяет, что createMovieReactionKeyboard(movieId) привязывает ID фильма к callback_data.
 */
TEST(KeyboardFactoryTest, MovieReactionKeyboardBindsMovieId) {
    auto defaultKb = bot::KeyboardFactory::createMovieReactionKeyboard();
    const auto* defaultLike = defaultKb.findByCallback("rec:like");
    ASSERT_NE(defaultLike, nullptr);
    EXPECT_EQ(defaultLike->callbackData, "rec:like");

    auto boundKb = bot::KeyboardFactory::createMovieReactionKeyboard(42);
    const auto* boundLike = boundKb.findByCallback("rec:like");
    ASSERT_NE(boundLike, nullptr);
    EXPECT_EQ(boundLike->callbackData, "rec:like:42");

    const auto* boundDislike = boundKb.findByCallback("rec:dislike");
    ASSERT_NE(boundDislike, nullptr);
    EXPECT_EQ(boundDislike->callbackData, "rec:dislike:42");

    const auto* boundNext = boundKb.findByCallback("rec:next");
    ASSERT_NE(boundNext, nullptr);
    EXPECT_EQ(boundNext->callbackData, "rec:next:42");

    const auto* boundRate = boundKb.findByCallback("rec:rate");
    ASSERT_NE(boundRate, nullptr);
    EXPECT_EQ(boundRate->callbackData, "rec:rate:42");

    const auto* menuBtn = boundKb.findByCallback("rec:menu");
    ASSERT_NE(menuBtn, nullptr);
    EXPECT_EQ(menuBtn->callbackData, "rec:menu");
}

