#pragma once

#include <cstdint>
#include <optional>

namespace api::types {

struct BirthDate {
    /**
     * @brief Day of the user's birth; 1-31
     */
    std::uint8_t day;

    /**
     * @brief Month of the user's birth; 1-12
     */
    std::uint8_t month;

    /**
     * @brief Optional. Year of the user's birth
     */
    std::optional<std::uint16_t> year;
};

}  // namespace api::types