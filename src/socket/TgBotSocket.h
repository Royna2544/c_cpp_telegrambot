#pragma once

#include <Types.h>

#include <array>
#include <climits>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#define SOCKET_PATH "/tmp/tgbot_sock"

enum TgBotCommand {
    CMD_WRITE_MSG_TO_CHAT_ID,
    CMD_EXIT,
    CMD_CTRL_SPAMBLOCK,
    CMD_OBSERVE_CHAT_ID,
    CMD_SEND_FILE_TO_CHAT_ID,
    CMD_MAX,
};

enum FileType {
    TYPE_PHOTO,
    TYPE_VIDEO,
    TYPE_GIF,
    TYPE_DOCUMENT,
    TYPE_MAX
};

template <typename T, typename V>
using ConstArrayElem = std::pair<T, V>;
template <typename T, typename V, int size>
using ConstArray = std::array<ConstArrayElem<T, V>, size>;

std::string toStr(TgBotCommand cmd);
int toCount(TgBotCommand cmd);
std::string toHelpText(void);

namespace TgBotCommandData {
struct WriteMsgToChatId {
    ChatId to;       // destination chatid
    char msg[2048];  // Msg to send
};

using Exit = void *;

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

}  // namespace TgBotCommandData

union TgBotCommandUnion {
    TgBotCommandData::WriteMsgToChatId data_1;
    TgBotCommandData::Exit data_2;  // unused
    TgBotCommandData::CtrlSpamBlock data_3;
    TgBotCommandData::ObserveChatId data_4;
    TgBotCommandData::SendFileToChatId data_5;
};

struct TgBotConnection {
    TgBotCommand cmd;
    union TgBotCommandUnion data;
};

using listener_callback_t = std::function<void(struct TgBotConnection)>;

void startListening(const listener_callback_t &cb);
void writeToSocket(const struct TgBotConnection &conn);
