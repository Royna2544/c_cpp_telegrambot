#pragma once

#include <cstdint>

namespace TgBotSocket {

/**
 * @brief Command enumeration for socket protocol
 */
enum class Command : std::int32_t {
    CMD_INVALID = 0,

    // Client commands (1-99)
    CMD_WRITE_MSG_TO_CHAT_ID = 1,
    CMD_CTRL_SPAMBLOCK = 2,
    CMD_OBSERVE_CHAT_ID = 3,
    CMD_SEND_FILE_TO_CHAT_ID = 4,
    CMD_OBSERVE_ALL_CHATS = 5,
    CMD_GET_UPTIME = 6,
    CMD_TRANSFER_FILE = 7,
    CMD_TRANSFER_FILE_REQUEST = 8,
    CMD_CLIENT_MAX = CMD_TRANSFER_FILE_REQUEST,

    // Server internal commands (100+)
    CMD_SERVER_INTERNAL_START = 100,
    CMD_GET_UPTIME_CALLBACK = CMD_SERVER_INTERNAL_START,
    CMD_GENERIC_ACK = 101,
    CMD_OPEN_SESSION = 102,
    CMD_OPEN_SESSION_ACK = 103,
    CMD_CLOSE_SESSION = 104,
    CMD_TRANSFER_FILE_BEGIN = 105,
    CMD_TRANSFER_FILE_CHUNK = 106,
    CMD_TRANSFER_FILE_CHUNK_RESPONSE = 107,
    CMD_TRANSFER_FILE_END = 108,
    CMD_MAX,
};

/**
 * @brief Payload encoding type
 */
enum class PayloadType : int32_t {
    Binary = 0,  ///< Packed C++ structs
    Json = 1     ///< UTF-8 JSON strings
};

}  // namespace TgBotSocket