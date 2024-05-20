#pragma once

// A header export for the TgBot's socket connection

#include <cstring>
#include <string>
#include "../../include/Types.h"

#ifndef TGBOT_NAMESPACE_BEGIN
#define TGBOT_NAMESPACE_BEGIN
#endif
#ifndef TGBOT_NAMESPACE_END
#define TGBOT_NAMESPACE_END
#endif
#undef ERROR_INVALID_STATE

constexpr int MAX_PATH_SIZE = 256;
constexpr int MAX_MSG_SIZE = 256;

enum TgBotCommand : std::int32_t {
    CMD_WRITE_MSG_TO_CHAT_ID,
    CMD_CTRL_SPAMBLOCK,
    CMD_OBSERVE_CHAT_ID,
    CMD_SEND_FILE_TO_CHAT_ID,
    CMD_OBSERVE_ALL_CHATS,
    CMD_DELETE_CONTROLLER_BY_ID,
    CMD_GET_UPTIME,
    CMD_UPLOAD_FILE,
    CMD_DOWNLOAD_FILE,
    CMD_CLIENT_MAX,

    // Below are internal commands
    CMD_SERVER_INTERNAL_START = 100,
    CMD_GET_UPTIME_CALLBACK = CMD_SERVER_INTERNAL_START,
    CMD_GENERIC_ACK,
    CMD_DOWNLOAD_FILE_CALLBACK,
    CMD_MAX,
};

TGBOT_NAMESPACE_BEGIN

enum FileType {
    TYPE_PHOTO,
    TYPE_VIDEO,
    TYPE_GIF,
    TYPE_DOCUMENT,
    TYPE_DICE,
    TYPE_MAX
};

enum CtrlSpamBlock {
    CTRL_OFF,              // Disabled
    CTRL_LOGGING_ONLY_ON,  // Logging only, not taking action
    CTRL_ON,               // Enabled, does delete but doesn't mute
    CTRL_ENFORCE,          // Enabled, deletes and mutes
    CTRL_MAX,
};

struct WriteMsgToChatId {
    ChatId to;      // destination chatid
    char msg[MAX_MSG_SIZE];  // Msg to send
};

struct ObserveChatId {
    ChatId id;
    bool observe;  // new state for given ChatId,
                   // true/false - Start/Stop observing
};

struct SendFileToChatId {
    ChatId id;           // Destination ChatId
    FileType type;       // File type for file
    char filepath[MAX_PATH_SIZE];  // Path to file
};

using ObserveAllChats = bool;

using DeleteControllerById = int;

using GetUptimeCallback = char[sizeof("Uptime: 99h 99m 99s")];

enum class AckType {
    SUCCESS,
    ERROR_TGAPI_EXCEPTION,
    ERROR_INVALID_ARGUMENT,
    ERROR_COMMAND_IGNORED,
    ERROR_RUNTIME_ERROR,
};

struct GenericAck {
    AckType result;                   // result type
    char error_msg[MAX_MSG_SIZE] {};  // Error message, only valid when result type is not SUCCESS

    // Create a new instance of the Generic Ack, error.
    explicit GenericAck(AckType result, const std::string& errorMsg) : result(result) {
        std::strncpy(this->error_msg, errorMsg.c_str(), MAX_MSG_SIZE);
        this->error_msg[MAX_MSG_SIZE - 1] = '\0';
    }
    // Create a new instance of the Generic Ack, success.
    explicit GenericAck() : result(AckType::SUCCESS) {
        std::strncpy(error_msg, "OK", MAX_MSG_SIZE);
    }
};

using UploadFile = char[MAX_PATH_SIZE];  // Destination file name

struct DownloadFile {
    char filepath[MAX_PATH_SIZE] {};  // Path to file (in remote)
    char destfilename[MAX_PATH_SIZE] {};  // Destination file name
};

TGBOT_NAMESPACE_END

/**
 * @brief Header for TgBotCommand Packets
 *
 * Header contains the magic value, command, and the size of the data
 */
struct TgBotCommandPacketHeader {
    using length_type = uint64_t;
    constexpr static int64_t MAGIC_VALUE = 0xDEADFACE;  ///< Magic value to identify the packet

    int64_t magic = MAGIC_VALUE;  ///< Magic value to identify the packet
    TgBotCommand cmd{};           ///< Command to be executed
    length_type data_size{};      ///< Size of the data in the packet
};