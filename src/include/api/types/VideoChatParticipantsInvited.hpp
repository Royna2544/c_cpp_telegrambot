#pragma once

#include <api/types/User.hpp>
#include <vector>

namespace api::types {

/**
 * @brief This object represents a service message about new members invited to
 * a video chat.
 */
struct VideoChatParticipantsInvited {
    /**
     * @brief New members that were invited to the video chat
     */
    std::vector<User> users;
};

}  // namespace api::types