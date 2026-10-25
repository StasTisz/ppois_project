#include <gtest/gtest.h>
#include "bot/IBotMessenger.h"
#include "bot/BotEventRouter.h"

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
