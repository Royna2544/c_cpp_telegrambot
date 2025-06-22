#pragma once

// A header export for the TgBot's socket connection
#include <api/typedefs.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <SharedMalloc.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

#include <flatbuffers/PacketHeader_generated.h>

template <typename T, size_t size>
inline bool arraycmp(const std::array<T, size>& lhs,
                     const std::array<T, size>& rhs) {
    if constexpr (std::is_same_v<T, char>) {
        return std::strncmp(lhs.data(), rhs.data(), size) == 0;
    }
    return std::memcmp(lhs.data(), rhs.data(), size) == 0;
}

template <size_t size>
inline void copyTo(std::array<char, size>& arr_in, const std::string_view buf) {
    strncpy(arr_in.data(), buf.data(), std::min(size - 1, buf.size()));
    arr_in[size - 1] = '\0';
}

template <size_t size, size_t size2>
inline void copyTo(std::array<char, size>& arr_in,
                   std::array<char, size2> buf) {
    static_assert(size >= size2,
                  "Destination must be same or bigger than source size");
    copyTo(arr_in, buf.data());
    arr_in[size - 1] = '\0';
}

namespace TgBotSocket {

/**
 * @brief Packet for sending commands to the server
 *
 * This packet is used to send commands to the server.
 * It contains a header, which contains the magic value, the command, and the
 * size of the data. The data is then followed by the actual data.
 */
struct Packet {
    using hmac_type = std::array<uint8_t, SHA256_DIGEST_LENGTH>;

    /**
     * @brief Header for TgBotCommand Packets
     *
     * Header contains the magic value, command, and the size of the data
     */
    struct Header {
        constexpr static int64_t MAGIC_VALUE_BASE = 0xDEADBEEF;
        // Version number, to be increased on breaking changes
        // 1: Initial version
        constexpr static int DATA_VERSION = 1;
        constexpr static int64_t MAGIC_VALUE = MAGIC_VALUE_BASE + DATA_VERSION;

        using length_type = uint32_t;
        using nounce_type = uint64_t;

        // Using AES-GCM
        constexpr static int IV_LENGTH = 12;
        constexpr static int TAG_LENGTH = 16;
        constexpr static int SESSION_TOKEN_LENGTH = 32;

        using session_token_type = std::array<char, SESSION_TOKEN_LENGTH>;
        using init_vector_type = std::array<uint8_t, IV_LENGTH>;
    };
    SharedMalloc header;
    SharedMalloc data;
    hmac_type hmac{};
};

// This border byte is used to distinguish JSON meta and file data
constexpr uint8_t JSON_BYTE_BORDER = 0xFF;
}
