#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <Types.h>

#define SOCKET_PATH "/tmp/tgbot_sock"

enum TgBotCommand {
    CMD_WRITE_MSG_TO_CHAT_ID,
    CMD_EXIT,
    CMD_CTRL_SPAMBLOCK,
    CMD_MAX,
};

#define ENUM_STR(enum) { enum, #enum }
extern std::unordered_map<TgBotCommand, std::string> kTgBotCommandStrMap;
static inline std::string toStr(TgBotCommand cmd) {
    return kTgBotCommandStrMap[cmd];
}

namespace TgBotCommandData {
struct WriteMsgToChatId {
    ChatId to; // destination chatid
    char msg[2048]; // Msg to send
};

using Exit = void *;

enum CtrlSpamBlock {
    CTRL_OFF, // Disabled
    CTRL_LOGGING_ONLY_ON, // Logging only, not taking action
    CTRL_ON, // Enabled
    CTRL_MAX,
};
}  // namespace TgBotCommandData

union TgBotCommandUnion {
    TgBotCommandData::WriteMsgToChatId data_1;
    TgBotCommandData::Exit data_2; // unused
    TgBotCommandData::CtrlSpamBlock data_3;        
};

struct TgBotConnection {
    TgBotCommand cmd;
    union TgBotCommandUnion data;
};

using listener_callback_t = std::function<void(struct TgBotConnection)>;

void startListening(listener_callback_t cb);
void writeToSocket(struct TgBotConnection cn);
