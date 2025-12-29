#pragma once

#include <bit>
#include <cstdint>
#include <type_traits>
#include <fmt/format.h>

namespace TgBotSocket {

/**
 * @brief C++20-compatible byteswap implementation
 * 
 * Replaces std::byteswap (C++23) with a C++20 compatible version.
 */
template <typename T>
    requires std::is_integral_v<T>
constexpr T byteswap(T value) noexcept {
    if constexpr (sizeof(T) == 1) {
        return value;
    } else if constexpr (sizeof(T) == 2) {
        return static_cast<T>((value >> 8) | (value << 8));
    } else if constexpr (sizeof(T) == 4) {
        return static_cast<T>(
            ((value & 0x000000FFU) << 24) |
            ((value & 0x0000FF00U) << 8) |
            ((value & 0x00FF0000U) >> 8) |
            ((value & 0xFF000000U) >> 24)
        );
    } else if constexpr (sizeof(T) == 8) {
        return static_cast<T>(
            ((value & 0x00000000000000FFULL) << 56) |
            ((value & 0x000000000000FF00ULL) << 40) |
            ((value & 0x0000000000FF0000ULL) << 24) |
            ((value & 0x00000000FF000000ULL) << 8) |
            ((value & 0x000000FF00000000ULL) >> 8) |
            ((value & 0x0000FF0000000000ULL) >> 24) |
            ((value & 0x00FF000000000000ULL) >> 40) |
            ((value & 0xFF00000000000000ULL) >> 56)
        );
    } else {
        static_assert(sizeof(T) <= 8, "byteswap only supports up to 64-bit types");
    }
}

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
                byteswap(static_cast<IntType>(value)));
        else
            return value;
    }

    constexpr operator IntType() const 
        requires (!std::is_same_v<Underlying, IntType>) {
        if constexpr (std::endian::native == std::endian::little)
            return byteswap(static_cast<IntType>(value));
        else
            return static_cast<IntType>(value);
    }

    constexpr ByteHelper(Underlying v) {
        if constexpr (std::endian::native == std::endian::little)
            value = static_cast<Underlying>(
                byteswap(static_cast<IntType>(v)));
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