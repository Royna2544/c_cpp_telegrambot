#pragma once

#include "ByteHelper.hpp"
#include "CoreTypes.hpp"
#include "Packet.hpp"
#include "Utilities.hpp"

#include <string>

namespace TgBotSocket::callback {

/**
 * @brief Acknowledgement result types
 */
enum class AckType {
    SUCCESS = 0,
    ERROR_TGAPI_EXCEPTION = 1,
    ERROR_INVALID_ARGUMENT = 2,
    ERROR_COMMAND_IGNORED = 3,
    ERROR_RUNTIME_ERROR = 4,
    ERROR_CLIENT_ERROR = 5,
};

/**
 * @brief Uptime query callback data
 * 
 * JSON schema: 
 * {
 *   "start_time": timestamp,
 *   "current_time": timestamp,
 *   "uptime": string
 * }
 */
struct alignas(Limits::ALIGNMENT) GetUptimeCallback {
    std::array<char, sizeof("Uptime: 999h 99m 99s")> uptime;
};

/**
 * @brief Generic acknowledgement for all commands
 * 
 * JSON schema:
 * {
 *   "result": bool,
 *   "error_type": string (optional),
 *   "error_msg": string (optional)
 * }
 */
struct alignas(Limits::ALIGNMENT) GenericAck {
    ByteHelper<AckType> result{};
    alignas(Limits::ALIGNMENT) MessageStringArray error_msg{};

    explicit GenericAck(AckType result, const std::string& errorMsg)
        : result(result) {
        copyTo(error_msg, errorMsg);
    }

    GenericAck() = default;

    static GenericAck ok() { 
        return GenericAck(AckType::SUCCESS, "OK"); 
    }
};

}  // namespace TgBotSocket::callback