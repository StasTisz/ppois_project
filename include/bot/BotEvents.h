#pragma once

#include <cstdint>
#include <string>

/**
 * @file BotEvents.h
 * @brief Data Transfer Objects (DTO) modeling incoming Telegram events for the bot layer.
 */

namespace bot {

/**
 * @struct IncomingMessage
 * @brief Immutable representation of an incoming text message from a Telegram user.
 *
 * Decouples domain routing and state machine handlers from library-specific message types
 * (such as `TgBot::Message::Ptr`), enabling deterministic unit testing with mock events.
 */
struct IncomingMessage {
    /// Unique identifier for the Telegram chat/dialog where the message originated.
    int64_t chatId{0};

    /// Unique Telegram identifier of the sending user.
    int64_t userId{0};

    /// Optional Telegram username of the sender (without leading '@'), empty if not set.
    std::string username;

    /// UTF-8 encoded text payload of the user message.
    std::string text;
};

/**
 * @struct IncomingCallback
 * @brief Immutable representation of a callback query triggered by an inline keyboard button.
 *
 * Contains query identifier needed for network acknowledgement, context message metadata,
 * and button payload data for router and FSM state transition handling.
 */
struct IncomingCallback {
    /// Unique query identifier assigned by Telegram to acknowledge the button click.
    std::string callbackId;

    /// Chat identifier containing the message with the clicked inline keyboard.
    int64_t chatId{0};

    /// Telegram user identifier of the person who pressed the button.
    int64_t userId{0};

    /// Message identifier of the message containing the clicked inline button.
    int32_t messageId{0};

    /// Data payload associated with the inline button (e.g., action code or parameter).
    std::string data;
};

} // namespace bot
