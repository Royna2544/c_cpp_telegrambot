#pragma once

#include <api/types/User.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace api::types {

/**
 * @brief Represents an invite link for a chat.
 */
struct ChatInviteLink {
    /**
     * @brief The invite link.
     * If the link was created by another chat administrator, then the second
     * part of the link will be replaced with “…”.
     */
    std::string inviteLink;

    /**
     * @brief Creator of the link
     */
    std::optional<User> creator;

    /**
     * @brief True, if users joining the chat via the link need to be approved
     * by chat administrators
     */
    bool createsJoinRequest;

    /**
     * @brief True, if the link is primary
     */
    bool isPrimary;

    /**
     * @brief True, if the link is revoked
     */
    bool isRevoked;

    /**
     * @brief Optional. Invite link name
     */
    std::optional<std::string> name;

    /**
     * @brief Optional. Point in time (Unix timestamp) when the link will expire
     * or has been expired
     */
    std::optional<std::uint32_t> expireDate;

    /**
     * @brief Optional. Maximum number of users that can be members of the chat
     * simultaneously after joining the chat via this invite link; 1-99999
     */
    std::optional<std::uint32_t> memberLimit;

    /**
     * @brief Optional. Number of pending join requests created using this link
     */
    std::optional<std::uint32_t> pendingJoinRequestCount;
};

}  // namespace api::types