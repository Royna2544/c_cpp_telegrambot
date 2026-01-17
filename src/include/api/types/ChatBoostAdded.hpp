#pragma once

#include <cstdint>

namespace api::types {

/**
 * @brief This object represents a service message about a user boosting a chat.
 */
struct ChatBoostAdded {
    /**
     * @brief Number of boosts added by the user
     */
    std::int32_t boostCount;
};

}  // name