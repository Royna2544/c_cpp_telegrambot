#pragma once

#include <array>
#include <cstring>
#include <string_view>
#include <algorithm>

namespace TgBotSocket {

/**
 * @brief Compare two std::array for equality
 */
template <typename T, size_t size>
inline bool arraycmp(const std::array<T, size>& lhs,
                     const std::array<T, size>& rhs) {
    if constexpr (std::is_same_v<T, char>) {
        return std::strncmp(lhs.data(), rhs.data(), size) == 0;
    }
    return std::memcmp(lhs.data(), rhs.data(), size) == 0;
}

/**
 * @brief Copy string_view to char array with null termination
 */
template <size_t size>
inline void copyTo(std::array<char, size>& arr_in, 
                   const std::string_view buf) {
    const size_t copy_size = std::min(size - 1, buf.size());
    std::copy_n(buf.data(), copy_size, arr_in.data());
    arr_in[copy_size] = '\0';
}

/**
 * @brief Copy char array to char array with null termination
 */
template <size_t size, size_t size2>
inline void copyTo(std::array<char, size>& arr_in,
                   const std::array<char, size2>& buf) {
    static_assert(size >= size2,
                  "Destination must be same or bigger than source size");
    copyTo(arr_in, std::string_view(buf.data()));
}

}  // namespace TgBotSocket