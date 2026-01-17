#pragma once

#include <cstdint>

namespace api::types {

/**
 * @brief This object represents a service message about a change in auto-delete
 * timer settings.
 */
struct MessageAutoDeleteTimerChanged {
    /**
     * @brief New auto-delete time for messages in the chat
     */
    std::int32_t messageAutoDeleteTime;
};

}  // namespace api::types