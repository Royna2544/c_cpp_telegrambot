#pragma once

#include <cstdint>

namespace api::types {

/**
 * @brief This object represents a service message about a video chat scheduled
 * in the chat.
 */
struct VideoChatScheduled {
    /**
     * @brief Point in time (Unix timestamp) when the video chat is supposed to
     * be started by a chat administrator
     */
    std::int32_t startDate;
};

}  // namespace api::types