#pragma once

#include <cstdint>
#include <string>

namespace api::types {

struct PollOption {
    /**
     * @brief Option text, 1-100 characters.
     */
    std::string text;

    /**
     * @brief Number of users that voted for this option.
     */
    std::int64_t voterCount;
};

}  // namespace api::types