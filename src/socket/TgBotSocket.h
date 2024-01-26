#pragma once

#include <Types.h>

#include <array>
#include <climits>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

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

template <typename T, typename V>
using ConstArrayElem = std::pair<T, V>;
template <typename T, typename V, int size>
using ConstArray = std::array<ConstArrayElem<T, V>, size>;

std::string TgBotCmd_toStr(TgBotCommand cmd);
int TgBotCmd_toCount(TgBotCommand cmd);
std::string TgBotCmd_getHelpText(void);

namespace TgBotCommandData {
struct WriteMsgToChatId {
    ChatId to;       // destination chatid
    char msg[2048];  // Msg to send
};

struct Exit {
    ExitOp op;       // operation desired
    char token[16];  // token data, used to verify exit op
};

enum CtrlSpamBlock {
    CTRL_OFF,              // Disabled
    CTRL_LOGGING_ONLY_ON,  // Logging only, not taking action
    CTRL_ON,               // Enabled
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
    char filepath[PATH_MAX];  // Path to file
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

struct TgBotConnection {
    TgBotCommand cmd;
    union TgBotCommandUnion data;
};

using listener_callback_t = std::function<void(struct TgBotConnection)>;

bool startListening(const listener_callback_t &cb);
void writeToSocket(struct TgBotConnection conn);
