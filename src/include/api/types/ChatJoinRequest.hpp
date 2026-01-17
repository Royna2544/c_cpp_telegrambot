#pragma once

#include <api/types/Chat.hpp>
#include <api/types/ChatInviteLink.hpp>
#include <api/types/User.hpp>

namespace api::types {

/**
 * @brief Represents a join request sent to a chat.
 */
struct ChatJoinRequest {
    /**
     * @brief Chat to which the request was sent
     */
    std::optional<Chat> chat;

    /**
     * @brief User that sent the join request
     */
    std::optional<User> from;

    /**
     * @brief Identifier of a private chat with the user who sent the join
     * request.
     *
     * This number may have more than 32 significant bits and some programming
     * languages may have difficulty/silent defects in interpreting it. But it
     * has at most 52 significant bits, so a 64-bit integer or double-precision
     * float type are safe for storing this identifier. The bot can use this
     * identifier for 5 minutes to send messages until the join request is
     * processed, assuming no other administrator contacted the user.
     */
    std::int64_t userChatId;

    /**
     * @brief Date the request was sent in Unix time
     */
    std::int32_t date;

    /**
     * @brief Optional. Bio of the user.
     */
    std::optional<std::string> bio;

    /**
     * @brief Optional. Chat invite link that was used by the user to send the
     * join request
     */
    std::optional<ChatInviteLink> inviteLink;
};

}  // namespace api::types