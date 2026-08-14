#pragma once

#include <cstddef>
#include <cstdint>

namespace imagep::limits {

// These limits keep a single public command from consuming most of a small
// bot host. A 16 MP RGBA still is at most 64 MiB before the rotation copy;
// video is deliberately tighter because multiple frames/codecs are alive at
// once.
inline constexpr std::uint64_t kMaxImagePixels = 16ULL * 1024 * 1024;
inline constexpr std::uint64_t kMaxVideoPixels = 4ULL * 1024 * 1024;
inline constexpr std::uint64_t kMaxDimension = 8192;
inline constexpr std::uint64_t kMaxFrames = 900;
inline constexpr std::uint64_t kMaxDurationSeconds = 30;
inline constexpr std::uint64_t kMaxFrameRate = 120;
inline constexpr std::uint64_t kMaxCompressedInputBytes = 20ULL * 1024 * 1024;
inline constexpr std::uint64_t kMaxOutputBytes = 20ULL * 1024 * 1024;

constexpr bool dimensionsAllowed(std::int64_t width, std::int64_t height,
                                 std::uint64_t maxPixels) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    const auto w = static_cast<std::uint64_t>(width);
    const auto h = static_cast<std::uint64_t>(height);
    return w <= kMaxDimension && h <= kMaxDimension && w <= maxPixels / h;
}

constexpr bool imageDimensionsAllowed(std::int64_t width, std::int64_t height) {
    return dimensionsAllowed(width, height, kMaxImagePixels);
}

constexpr bool videoDimensionsAllowed(std::int64_t width, std::int64_t height) {
    return dimensionsAllowed(width, height, kMaxVideoPixels);
}

constexpr bool compressedSizeAllowed(std::int64_t bytes) {
    return bytes >= 0 &&
           static_cast<std::uint64_t>(bytes) <= kMaxCompressedInputBytes;
}

}  // namespace imagep::limits
