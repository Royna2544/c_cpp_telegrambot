#pragma once

#include "forwards.hpp"

namespace tgbot_api {

/**
 * @brief This object represents a chat.
 */
class Chat {
   public:
    using Ptr = Chat*;

    /**
     * @brief Enum of possible types of a chat.
     */
    enum class Type { Private, Group, Supergroup, Channel };

    /**
     * @brief Unique identifier for this chat.
     */
    ChatId id{};

    /**
     * @brief Type of chat
     */
    Type type{Type::Private};

    /**
     * @brief Optional. Title, for supergroups, channels and group chats
     */
    std::optional<std::string> title;

    /**
     * @brief Optional. Username, for private chats, supergroups and channels
     */
    std::optional<std::string> username;

    /**
     * @brief Optional. First name of the other party in a private chat
     */
    std::optional<std::string> firstName;

    /**
     * @brief Optional. Last name of the other party in a private chat
     */
    std::optional<std::string> lastName;

    /**
     * @brief Optional. True, if the supergroup chat is a forum
     */
    std::optional<bool> isForum;
};

}  // namespace tgbot_api
