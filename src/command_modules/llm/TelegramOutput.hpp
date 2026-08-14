#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace llm::telegram_output {

inline constexpr std::size_t kTelegramTextLimit = 4096;
// MarkdownV2 escaping can double every input byte. Keeping raw chunks at 2000
// leaves room under Telegram's 4096-character limit after escaping.
inline constexpr std::size_t kRawMarkdownChunkBytes = 2000;
inline constexpr std::size_t kPlainChunkBytes = 4000;
inline constexpr std::size_t kMaxTelegramChunks = 4;
inline constexpr std::string_view kTruncatedMarker = "\n[response truncated]";

inline bool isUtf8Continuation(unsigned char ch) {
    return (ch & 0xC0U) == 0x80U;
}

inline std::vector<std::string> splitUtf8(std::string_view text,
                                          std::size_t maxBytes) {
    std::vector<std::string> chunks;
    if (text.empty()) {
        return chunks;
    }
    // A valid UTF-8 code point is at most four bytes.
    maxBytes = std::max<std::size_t>(maxBytes, 4);

    for (std::size_t start = 0; start < text.size();) {
        std::size_t end = std::min(start + maxBytes, text.size());
        if (end < text.size()) {
            while (end > start &&
                   isUtf8Continuation(static_cast<unsigned char>(text[end]))) {
                --end;
            }
        }
        if (end == start) {
            // This is only reachable for malformed UTF-8; make forward
            // progress without reading past the input.
            end = std::min(start + maxBytes, text.size());
        }
        chunks.emplace_back(text.substr(start, end - start));
        start = end;
    }
    return chunks;
}

inline std::vector<std::string> splitForMarkdown(std::string_view text) {
    const auto maxBytes = kRawMarkdownChunkBytes * kMaxTelegramChunks;
    if (text.size() <= maxBytes) {
        return splitUtf8(text, kRawMarkdownChunkBytes);
    }
    std::size_t end = maxBytes - kTruncatedMarker.size();
    while (end > 0 &&
           isUtf8Continuation(static_cast<unsigned char>(text[end]))) {
        --end;
    }
    std::string bounded(text.substr(0, end));
    bounded += kTruncatedMarker;
    return splitUtf8(bounded, kRawMarkdownChunkBytes);
}

inline std::vector<std::string> splitPlain(std::string_view text) {
    const auto maxBytes = kPlainChunkBytes * kMaxTelegramChunks;
    if (text.size() <= maxBytes) {
        return splitUtf8(text, kPlainChunkBytes);
    }
    std::size_t end = maxBytes - kTruncatedMarker.size();
    while (end > 0 &&
           isUtf8Continuation(static_cast<unsigned char>(text[end]))) {
        --end;
    }
    std::string bounded(text.substr(0, end));
    bounded += kTruncatedMarker;
    return splitUtf8(bounded, kPlainChunkBytes);
}

}  // namespace llm::telegram_output
