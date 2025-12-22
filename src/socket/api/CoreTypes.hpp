#pragma once

#include <cstdint>

namespace TgBotSocket {

/**
 * @brief Protocol version and magic values
 */
namespace Protocol {
    constexpr int64_t MAGIC_VALUE_BASE = 0xDEADFACE;
    constexpr int DATA_VERSION = 13;
    constexpr int64_t MAGIC_VALUE = MAGIC_VALUE_BASE + DATA_VERSION;
}  // namespace Protocol

/**
 * @brief Cryptographic constants
 */
namespace Crypto {
    constexpr int IV_LENGTH = 12;
    constexpr int TAG_LENGTH = 16;
    constexpr int SESSION_TOKEN_LENGTH = 32;
}  // namespace Crypto

/**
 * @brief Data structure alignment and size constants
 */
namespace Limits {
    constexpr int MAX_PATH_SIZE = 256;
    constexpr int MAX_MSG_SIZE = 256;
    constexpr int ALIGNMENT = 8;
}  // namespace Limits

}  // namespace TgBotSocket