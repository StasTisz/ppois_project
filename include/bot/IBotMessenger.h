#pragma once

#include <cstdint>
#include <string>

/**
 * @file IBotMessenger.h
 * @brief Abstract interface defining outbound communication capabilities for Telegram bot components.
 */

namespace bot {

/**
 * @class IBotMessenger
 * @brief Contract isolating domain and UI presentation from concrete Telegram networking libraries.
 *
 * Provides operations to dispatch outbound text messages and answer callback queries.
 * Implementing this interface allows swapping real network clients (e.g. `TgBotMessenger`)
 * with test doubles (e.g. `MockBotMessenger`) in test suites.
 */
class IBotMessenger {
public:
    /**
     * @brief Virtual destructor ensuring safe polymorphic deletion of messenger implementations.
     */
    virtual ~IBotMessenger() = default;

    /**
     * @brief Dispatches a plain text message to a specific Telegram chat.
     * @param chatId Unique identifier for the target chat or channel.
     * @param text Text payload to be sent (formatted as UTF-8).
     */
    virtual void sendMessage(int64_t chatId, const std::string& text) = 0;

    /**
     * @brief Sends an acknowledgement response for an incoming callback query.
     *
     * Calling this method halts the progress spinner on the user's Telegram client after clicking
     * an inline keyboard button. Optionally displays a brief notification or toast.
     *
     * @param callbackId Unique identifier of the callback query being acknowledged.
     * @param text Optional text to display as a notification popup to the user.
     */
    virtual void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") = 0;
};

} // namespace bot
