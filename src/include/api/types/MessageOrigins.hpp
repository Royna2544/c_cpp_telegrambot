#pragma once

#include <api/types/Chat.hpp>
#include <api/types/User.hpp>
#include <cstdint>
#include <string>
#include <optional>
#include <variant>

namespace api::types {

/**
 * @brief The message was originally sent to a channel chat.
 */
struct MessageOriginChannel {
    /**
     * @brief Type of the origin, must be "channel"
     */
    std::string type;

    /**
     * @brief Channel chat to which the message was originally sent
     */
    Chat chat;

    /**
     * @brief Unique message identifier inside the chat
     */
    std::int32_t messageId{};

    /**
     * @brief Optional. Signature of the original post author
     */
    std::optional<std::string> authorSignature;
};


/**
 * @brief The message was originally sent on behalf of a chat to a group chat.
 */
struct MessageOriginChat {
    /**
     * @brief Type of the origin, must be "chat"
     */
    std::string type;

    /**
     * @brief Chat that sent the message originally
     */
    Chat senderChat;

    /**
     * @brief Optional. For messages originally sent by an anonymous chat
     * administrator, original message author signature
     */
    std::optional<std::string> authorSignature;
};


/**
 * @brief The message was originally sent by an unknown user.
 */
struct MessageOriginHiddenUser {
    /**
     * @brief Type of the origin, must be "hidden_user"
     */
    std::string type;

    /**
     * @brief Name of the user that sent the message originally
     */
    std::string senderUserName;
};


/**
 * @brief The message was originally sent by a known user.
 */
struct MessageOriginUser {
    /**
     * @brief Type of the origin, must be "user"
     */
    std::string type;

    /**
     * @brief User that sent the message originally
     */
    User senderUser;
};

using MessageOrigin = std::variant<
    MessageOriginChannel,
    MessageOriginChat,
    MessageOriginHiddenUser, MessageOriginUser>;

}