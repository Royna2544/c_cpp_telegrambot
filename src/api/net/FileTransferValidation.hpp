#pragma once

#include <cstddef>
#include <cstdint>

// Pure boundary/size helpers for the file-transfer RPCs. Extracted from
// SocketServiceImpl so the bounds and overflow checks can be unit-tested
// without standing up the gRPC service.
namespace tgbot::socket::transfer {

inline constexpr std::uintmax_t kChunkSize = 64 * 1024;        // 64 KB
inline constexpr std::uintmax_t kMaxTransferSize = 2ULL << 30;  // 2 GiB

// Number of chunks needed to cover a file of `size` bytes.
inline int chunkCount(std::uintmax_t size) {
    return static_cast<int>((size + kChunkSize - 1) / kChunkSize);
}

// Reject client-supplied sizes above the cap (prevents disk exhaustion).
inline bool sizeWithinCap(std::uintmax_t size) {
    return size <= kMaxTransferSize;
}

// An upload chunk write must start at a non-negative offset and stay within the
// declared file size (offset + data <= total), without integer overflow.
inline bool uploadChunkInRange(std::int64_t offset, std::size_t dataSize,
                               std::uintmax_t totalSize) {
    if (offset < 0) {
        return false;
    }
    const auto off = static_cast<std::uintmax_t>(offset);
    // off + dataSize > totalSize, rearranged to avoid overflow.
    if (off > totalSize) {
        return false;
    }
    return dataSize <= totalSize - off;
}

// A download chunk index must fall within the file's chunk count.
inline bool downloadChunkValid(std::int64_t chunkIdx, int chunkCount) {
    return chunkIdx >= 0 && chunkIdx < chunkCount;
}

// Byte offset of a download chunk.
inline std::uintmax_t chunkOffset(std::int64_t chunkIdx) {
    return static_cast<std::uintmax_t>(chunkIdx) * kChunkSize;
}

}  // namespace tgbot::socket::transfer
