#pragma once

#include <bit>
#include <cstdint>
#include <type_traits>
#include <fmt/format.h>

namespace TgBotSocket {

/**
 * @brief Helper to safely extract underlying type ONLY if it's an enum
 */
template <typename T, bool IsEnum = std::is_enum_v<T>>
struct SafeUnderlying {
    using type = T;
};

template <typename T>
struct SafeUnderlying<T, true> {
    using type = std::underlying_type_t<T>;
};

/**
 * @brief Wrapper for automatic endianness conversion
 * 
 * Converts values to little-endian wire format automatically.
 * Works with both integer types and enums.
 */
template <typename Underlying>
struct ByteHelper {
    Underlying value;
    using type = Underlying;
    using IntType = typename SafeUnderlying<Underlying>::type;

    constexpr operator Underlying() const {
        if constexpr (std::endian::native == std::endian::little)
            return static_cast<Underlying>(
                std::byteswap(static_cast<IntType>(value)));
        else
            return value;
    }

    constexpr operator IntType() const 
        requires (!std::is_same_v<Underlying, IntType>) {
        if constexpr (std::endian::native == std::endian::little)
            return std::byteswap(static_cast<IntType>(value));
        else
            return static_cast<IntType>(value);
    }

    constexpr ByteHelper(Underlying v) {
        if constexpr (std::endian::native == std::endian::little)
            value = static_cast<Underlying>(
                std::byteswap(static_cast<IntType>(v)));
        else
            value = v;
    }

    ByteHelper() : value{} {}

    ByteHelper& operator=(Underlying v) {
        *this = ByteHelper(v);
        return *this;
    }
};

}  // namespace TgBotSocket

/**
 * @brief fmt formatter for ByteHelper
 */
template <typename T>
struct fmt::formatter<TgBotSocket::ByteHelper<T>> : fmt::formatter<T> {
    template <typename FormatContext>
    auto format(const TgBotSocket::ByteHelper<T>& wrapper, 
                FormatContext& ctx) const {
        return fmt::formatter<T>::format(static_cast<T>(wrapper), ctx);
    }
};