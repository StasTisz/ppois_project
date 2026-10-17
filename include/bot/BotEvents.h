#pragma once

#include <cstdint>
#include <string>

namespace bot {

/**
 * @brief Representation of an incoming user text message.
 */
struct IncomingMessage {
    int64_t chatId{0};
    int64_t userId{0};
    std::string username;
    std::string text;
};

/**
 * @brief Representation of an incoming inline button callback query.
 */
struct IncomingCallback {
    std::string callbackId;
    int64_t chatId{0};
    int64_t userId{0};
    int32_t messageId{0};
    std::string data;
};

} // namespace bot
