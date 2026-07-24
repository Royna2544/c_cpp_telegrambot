#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>

// Pure boundary/size helpers for the file-transfer RPCs. Extracted from
// SocketServiceImpl so the bounds and overflow checks can be unit-tested
// without standing up the gRPC service.
namespace tgbot::socket::transfer {

inline constexpr std::uintmax_t kChunkSize = 64 * 1024;        // 64 KB
inline constexpr std::uintmax_t kMaxTransferSize =
    5ULL * 1024 * 1024 * 1024 / 2;  // exactly 2.5 GiB

class UploadCoverage {
   public:
    explicit UploadCoverage(std::uintmax_t totalSize = 0)
        : totalSize_(totalSize) {}

    void add(std::uintmax_t offset, std::uintmax_t length) {
        if (length == 0) return;
        std::uintmax_t begin = offset;
        std::uintmax_t end = offset + length;
        auto it = ranges_.lower_bound(begin);
        if (it != ranges_.begin()) {
            auto previous = std::prev(it);
            if (previous->second >= begin) it = previous;
        }
        while (it != ranges_.end() && it->first <= end) {
            begin = std::min(begin, it->first);
            end = std::max(end, it->second);
            coveredBytes_ -= it->second - it->first;
            it = ranges_.erase(it);
        }
        ranges_[begin] = end;
        coveredBytes_ += end - begin;
    }

    [[nodiscard]] bool complete() const {
        return coveredBytes_ == totalSize_;
    }
    [[nodiscard]] std::uintmax_t coveredBytes() const { return coveredBytes_; }

   private:
    std::uintmax_t totalSize_{};
    std::uintmax_t coveredBytes_{};
    std::map<std::uintmax_t, std::uintmax_t> ranges_;
};

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
