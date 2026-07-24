#pragma once

#include <cstddef>
#include <limits>
#include <optional>

namespace imagep::webp {

inline std::optional<std::size_t> decodedByteSize(
    int width, int height, std::size_t maximumBytes) {
    constexpr std::size_t channels = 4;
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h) {
        return std::nullopt;
    }
    const auto pixels = w * h;
    if (pixels > maximumBytes / channels) {
        return std::nullopt;
    }
    return pixels * channels;
}

}  // namespace imagep::webp
