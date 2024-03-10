#pragma once

#include "../include/Types.h"
#include "../include/Logging.h"

#include <cstring>
#include <string>

#ifdef __ANDROID__
#define SOCKET_PATH "/data/data/com.termux/files/usr/tmp/tgbot_sock"
#elif defined __WIN32
#define SOCKET_PATH "C:\\Temp\\tgbot.sock"
#else
#define SOCKET_PATH "/tmp/tgbot_sock"
#endif

enum TgBotCommand {
    CMD_EXIT,
    CMD_WRITE_MSG_TO_CHAT_ID,
    CMD_CTRL_SPAMBLOCK,
    CMD_OBSERVE_CHAT_ID,
    CMD_SEND_FILE_TO_CHAT_ID,
    CMD_OBSERVE_ALL_CHATS,
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
    std::string toStr(TgBotCommand cmd);
    int toCount(TgBotCommand cmd);
    std::string getHelpText(void);
} // namespace TgBotCmd

namespace TgBotCommandData {
struct WriteMsgToChatId {
    ChatId to;       // destination chatid
    char msg[256];  // Msg to send
};

struct Exit {
    ExitOp op;       // operation desired
    char token[16];  // token data, used to verify exit op
    static Exit create(ExitOp op, const std::string& buf) {
        const int bufLen = sizeof(Exit::token) - 1;
        Exit e {};

        e.op = op;
        strncpy(e.token, buf.c_str(), bufLen);
        e.token[bufLen] = 0;
        if (buf.size() != bufLen)
            LOG_W("buf str doesn't fit token fully: tokenlen %d vs buflen %zu", bufLen, buf.size());
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
    ChatId id;                // Destination ChatId
    FileType type;            // File type for file
    char filepath[256];       // Path to file
};

using ObserveAllChats = bool;
}  // namespace TgBotCommandData

union TgBotCommandUnion {
    TgBotCommandData::WriteMsgToChatId data_1;
    TgBotCommandData::Exit data_2;  // unused
    TgBotCommandData::CtrlSpamBlock data_3;
    TgBotCommandData::ObserveChatId data_4;
    TgBotCommandData::SendFileToChatId data_5;
    TgBotCommandData::ObserveAllChats data_6;
};

constexpr int64_t MAGIC_VALUE = 0xDEADFACE;
struct TgBotConnection {
    TgBotCommand cmd;
    union TgBotCommandUnion data;
    int64_t magic = MAGIC_VALUE;
};