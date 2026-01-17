#pragma once

#include <cstdint>

namespace api::types {

/**
 * @brief This object represents a service message about a video chat ended in
 * the chat.
 */
struct VideoChatEnded {
    /**
     * @brief Video chat duration in seconds
     */
    std::int32_t duration;
};

}  // namespace api::types