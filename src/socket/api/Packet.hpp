#pragma once

#include "ByteHelper.hpp"
#include "Commands.hpp"
#include "CoreTypes.hpp"

#include <SharedMalloc.hpp>
#include <openssl/sha.h>
#include <array>
#include <cstdint>

namespace TgBotSocket {

/**
 * @brief Network packet structure for socket communication
 * 
 * Wire format:
 * 1. Header (80 bytes)
 * 2. Encrypted payload (variable)
 * 3. HMAC (32 bytes)
 */
struct alignas(Limits::ALIGNMENT) Packet {
    using hmac_type = std::array<uint8_t, SHA256_DIGEST_LENGTH>;

    /**
     * @brief Packet header (80 bytes, 8-byte aligned)
     */
    struct alignas(Limits::ALIGNMENT) Header {
        using length_type = uint32_t;
        using nounce_type = uint64_t;
        using session_token_type = std::array<char, Crypto::SESSION_TOKEN_LENGTH>;
        using init_vector_type = std::array<uint8_t, Crypto::IV_LENGTH>;

        ByteHelper<int64_t> magic = Protocol::MAGIC_VALUE;
        ByteHelper<Command> cmd;
        ByteHelper<PayloadType> data_type;
        ByteHelper<length_type> data_size;
        session_token_type session_token;
        ByteHelper<nounce_type> nonce;
        init_vector_type init_vector;
    } header{};

    SharedMalloc data;
    hmac_type hmac{};
};

// Type aliases for common array types
using PathStringArray = std::array<char, Limits::MAX_PATH_SIZE>;
using MessageStringArray = std::array<char, Limits::MAX_MSG_SIZE>;
using SHA256StringArray = std::array<uint8_t, SHA256_DIGEST_LENGTH>;

}  // namespace TgBotSocket