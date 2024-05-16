#pragma once

#include <absl/log/check.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <type_traits>

#include "../include/Types.h"
#include "SharedMalloc.hpp"
#include "SocketData.hpp"

inline std::filesystem::path getSocketPath() {
    static auto spath = std::filesystem::temp_directory_path() / "tgbot.sock";
    return spath;
}

enum TgBotCommand : std::int32_t {
    CMD_WRITE_MSG_TO_CHAT_ID,
    CMD_CTRL_SPAMBLOCK,
    CMD_OBSERVE_CHAT_ID,
    CMD_SEND_FILE_TO_CHAT_ID,
    CMD_OBSERVE_ALL_CHATS,
    CMD_DELETE_CONTROLLER_BY_ID,
    CMD_GET_UPTIME,
    CMD_CLIENT_MAX,

    // Below are internal commands
    CMD_SERVER_INTERNAL_START = 100,
    CMD_GET_UPTIME_CALLBACK = CMD_SERVER_INTERNAL_START,
    CMD_MAX,
};

enum FileType {
    TYPE_PHOTO,
    TYPE_VIDEO,
    TYPE_GIF,
    TYPE_DOCUMENT,
    TYPE_DICE,
    TYPE_MAX
};

enum ExitOp {
    SET_TOKEN,
    DO_EXIT,
};

namespace TgBotCmd {
/**
 * @brief Convert TgBotCommand to string
 *
 * @param cmd TgBotCommand to convert
 * @return std::string string representation of TgBotCommand enum
 */
std::string toStr(TgBotCommand cmd);

/**
 * @brief Get count of TgBotCommand
 *
 * @param cmd TgBotCommand to get arg count of
 * @return required arg count of TgBotCommand
 */
int toCount(TgBotCommand cmd);

/**
 * @brief Check if given command is a client command
 *
 * @param cmd TgBotCommand to check
 * @return true if given command is a client command, false otherwise
 */
bool isClientCommand(TgBotCommand cmd);

/**
 * @brief Check if given command is an internal command
 *
 * @param cmd TgBotCommand to check
 * @return true if given command is an internal command, false otherwise
 */
bool isInternalCommand(TgBotCommand cmd);

/**
 * @brief Get help text for TgBotCommand
 *
 * @return std::string help text for TgBotCommand
 */
std::string getHelpText(void);
}  // namespace TgBotCmd

namespace TgBotCommandData {
struct WriteMsgToChatId {
    ChatId to;      // destination chatid
    char msg[256];  // Msg to send
};

struct Exit {};

enum CtrlSpamBlock {
    CTRL_OFF,              // Disabled
    CTRL_LOGGING_ONLY_ON,  // Logging only, not taking action
    CTRL_ON,               // Enabled, does delete but doesn't mute
    CTRL_ENFORCE,          // Enabled, deletes and mutes
    CTRL_MAX,
};

struct ObserveChatId {
    ChatId id;
    bool observe;  // new state for given ChatId,
                   // true/false - Start/Stop observing
};

struct SendFileToChatId {
    ChatId id;           // Destination ChatId
    FileType type;       // File type for file
    char filepath[256];  // Path to file
};

using ObserveAllChats = bool;

using DeleteControllerById = int;

using GetUptimeCallback = char[sizeof("Uptime: 99h 99m 99s")];

}  // namespace TgBotCommandData

/**
 * @brief Header for TgBotCommand Packets
 *
 * Header contains the magic value, command, and the size of the data
 */
struct TgBotCommandPacketHeader {
    int64_t magic;                      ///< Magic value to identify the packet
    TgBotCommand cmd;                   ///< Command to be executed
    SocketData::length_type data_size;  ///< Size of the data in the packet
};

/**
 * @brief Packet for sending commands to the server
 *
 * @code data_ptr is only vaild for this scope: This should not be sent, instead
 * it must be memcpy'd
 *
 * This packet is used to send commands to the server.
 * It contains a header, which contains the magic value, the command, and the
 * size of the data. The data is then followed by the actual data.
 */
struct TgBotCommandPacket {
    static constexpr int64_t MAGIC_VALUE = 0xDEADFACE;
    static constexpr auto hdr_sz = sizeof(TgBotCommandPacketHeader);
    TgBotCommandPacketHeader header{};
    SharedMalloc data_ptr;

    explicit TgBotCommandPacket(SocketData::length_type length)
        : data_ptr(length) {
        header.magic = TgBotCommandPacket::MAGIC_VALUE;
        header.data_size = length;
    }

    // Constructor that takes malloc
    template <typename T>
    explicit TgBotCommandPacket(TgBotCommand cmd, T data)
        : TgBotCommandPacket(cmd, &data, sizeof(T)) {
        static_assert(!std::is_pointer_v<T>,
                      "This constructor should not be used with a pointer");
    }

    // Constructor that takes pointer, uses malloc but with size
    template <typename T>
    explicit TgBotCommandPacket(TgBotCommand cmd, T data, std::size_t size)
        : data_ptr(size) {
        static_assert(std::is_pointer_v<T>,
                      "This constructor should not be used with non pointer");
        header.cmd = cmd;
        header.magic = MAGIC_VALUE;
        header.data_size = size;
        memcpy(data_ptr.getData(), data, header.data_size);
    }
    SocketData toSocketData() {
        SocketData data(hdr_sz + header.data_size);
        void* dataBuf = data.data->getData();
        memcpy(dataBuf, &header, hdr_sz);
        memcpy(static_cast<char*>(dataBuf) + hdr_sz, data_ptr.getData(),
               header.data_size);
        return data;
    }
};