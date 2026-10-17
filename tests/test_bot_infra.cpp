#include <gtest/gtest.h>
#include "bot/IBotMessenger.h"
#include "bot/BotEventRouter.h"

#include <vector>
#include <string>
#include <memory>

namespace {

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

TEST(BotEventRouterTest, HandlesStartCommand) {
    auto mock = std::make_shared<MockBotMessenger>();
    bot::BotEventRouter router(mock);

    bot::IncomingMessage msg{123, 456, "stas", "/start"};
    router.onTextMessage(msg);

    ASSERT_EQ(mock->sentMessages.size(), 1);
    EXPECT_EQ(mock->sentMessages[0].chatId, 123);
    EXPECT_EQ(mock->sentMessages[0].text, "Welcome to Movie Recommendation Bot!");
}

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
