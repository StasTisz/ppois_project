#pragma once

#include <cstdint>
#include <string>

namespace bot {

/**
 * @brief Abstract interface for sending messages and acknowledging callbacks.
 * Decouples bot business logic from specific Telegram API libraries (e.g., tgbot-cpp).
 */
class IBotMessenger {
public:
    virtual ~IBotMessenger() = default;

    /**
     * @brief Sends a text message to a specified chat.
     * @param chatId Telegram chat identifier.
     * @param text Text content of the message.
     */
    virtual void sendMessage(int64_t chatId, const std::string& text) = 0;

    /**
     * @brief Acknowledges an incoming callback query to stop loading state on Telegram client.
     * @param callbackId Identifier of the callback query.
     * @param text Optional notification/alert text to display to user.
     */
    virtual void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") = 0;
};

} // namespace bot
