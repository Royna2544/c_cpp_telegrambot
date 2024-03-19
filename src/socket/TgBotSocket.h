#pragma once

#include <cstring>
#include <filesystem>
#include <string>

#include "../include/Logging.h"
#include "../include/Types.h"

inline std::filesystem::path getSocketPath() {
    static auto spath = std::filesystem::temp_directory_path() / "tgbot.sock";
    return spath;
}

enum TgBotCommand {
    CMD_EXIT,
    CMD_WRITE_MSG_TO_CHAT_ID,
    CMD_CTRL_SPAMBLOCK,
    CMD_OBSERVE_CHAT_ID,
    CMD_SEND_FILE_TO_CHAT_ID,
    CMD_OBSERVE_ALL_CHATS,
    CMD_DELETE_CONTROLLER_BY_ID,
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

struct Exit {
    static constexpr int TokenLen = 15;
    ExitOp op;             // operation desired
    char token[TokenLen + 1];  // token data, used to verify exit op
    static Exit create(ExitOp op, const std::string& buf) {
        Exit e{};

        e.op = op;
        strncpy(e.token, buf.c_str(), TokenLen);
        e.token[TokenLen] = 0;
        if (buf.size() != TokenLen)
            LOG(LogLevel::WARNING,
                "buf str doesn't fit token fully: tokenlen %d vs buflen %zu",
                TokenLen, buf.size());
        return e;
    }
};

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

}  // namespace TgBotCommandData

union TgBotCommandUnion {
    TgBotCommandData::WriteMsgToChatId data_1;
    TgBotCommandData::Exit data_2;  // unused
    TgBotCommandData::CtrlSpamBlock data_3;
    TgBotCommandData::ObserveChatId data_4;
    TgBotCommandData::SendFileToChatId data_5;
    TgBotCommandData::ObserveAllChats data_6;
    TgBotCommandData::DeleteControllerById data_7;
};

constexpr int64_t MAGIC_VALUE = 0xDEADFACE;
struct TgBotConnection {
    TgBotConnection() = default;
    TgBotConnection(TgBotCommand _cmd, union TgBotCommandUnion _data)
        : cmd(_cmd), data(_data) {}

    int64_t magic = MAGIC_VALUE;
    TgBotCommand cmd;
    union TgBotCommandUnion data;
};
