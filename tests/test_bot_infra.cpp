#include <gtest/gtest.h>
#include "bot/IBotMessenger.h"
#include "bot/BotEventRouter.h"

#include <vector>
#include <string>
#include <memory>

/**
 * @file test_bot_infra.cpp
 * @brief Unit tests verifying event routing, callback acknowledgement, and messaging abstraction.
 */

namespace {

/**
 * @class MockBotMessenger
 * @brief Test spy implementing `bot::IBotMessenger` to verify outbound messages in memory.
 */
class MockBotMessenger : public bot::IBotMessenger {
public:
    /// Record of a dispatched text message.
    struct SentMessage {
        int64_t chatId{0};
        std::string text;
    };

    /// Record of an acknowledged callback query.
    struct AnsweredCallback {
        std::string callbackId;
        std::string text;
    };

    /**
     * @brief Records message sending invocation.
     * @param chatId Destination chat.
     * @param text Message body.
     */
    void sendMessage(int64_t chatId, const std::string& text) override {
        sentMessages.push_back({chatId, text});
    }

    /**
     * @brief Records callback query acknowledgement invocation.
     * @param callbackId Target callback ID.
     * @param text Notification text.
     */
    void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") override {
        answeredCallbacks.push_back({callbackId, text});
    }

    /// History of all messages sent through this mock instance.
    std::vector<SentMessage> sentMessages;

    /// History of all callback acknowledgements recorded by this mock instance.
    std::vector<AnsweredCallback> answeredCallbacks;
};

} // namespace

/**
 * @test Verifies that the '/start' onboarding command outputs the welcome message to the sender.
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
 * @test Verifies that standard non-command text is passed correctly to registered message handlers.
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
 * @test Verifies that callback queries always trigger Telegram UI acknowledgement
 * and route their data payload to registered callback handlers.
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

    // Verify acknowledgement is sent to remove loading spinner in Telegram
    ASSERT_EQ(mock->answeredCallbacks.size(), 1);
    EXPECT_EQ(mock->answeredCallbacks[0].callbackId, "cb_1");

    // Verify payload reached custom handler
    EXPECT_TRUE(callbackHandlerCalled);
    EXPECT_EQ(capturedCallback.data, "genre:sci-fi");
    EXPECT_EQ(capturedCallback.chatId, 123);
    EXPECT_EQ(capturedCallback.userId, 456);
}

/**
 * @test Verifies that empty incoming messages are ignored and produce neither actions nor dispatches.
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
