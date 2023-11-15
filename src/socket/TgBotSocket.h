#pragma once

#include <Types.h>

#include <array>
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
    CMD_MAX,
};

#define ENUM_STR(enum) std::make_pair(enum, #enum)
#define ARGUMENT_SIZE(enum, len) std::make_pair(enum, len)

template <typename T, typename V>
using ConstArrayElem = std::pair<T, V>;
template <typename T, typename V, int size>
using ConstArray = std::array<ConstArrayElem<T, V>, size>;

template <typename T, int N, typename... V>
std::array<T, sizeof...(V)> make_array(V &&...v) {
    static_assert(sizeof...(V) == N, "Must match declared size");
    return {{std::forward<V>(v)...}};
}

extern const ConstArray<TgBotCommand, std::string, CMD_MAX - 1> kTgBotCommandStrMap;
extern const ConstArray<TgBotCommand, int, CMD_MAX - 1> kTgBotCommandArgsCount;

static inline std::string toStr(TgBotCommand cmd) {
    for (const auto &elem : kTgBotCommandStrMap) {
        if (elem.first == cmd) {
            return elem.second;
        }
    }
    return {};
}
static inline int toCount(TgBotCommand cmd) {
    for (const auto &elem : kTgBotCommandArgsCount) {
        if (elem.first == cmd) {
            return elem.second;
        }
    }
    return -1;
}

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

}  // namespace TgBotCommandData

union TgBotCommandUnion {
    TgBotCommandData::WriteMsgToChatId data_1;
    TgBotCommandData::Exit data_2;  // unused
    TgBotCommandData::CtrlSpamBlock data_3;
    TgBotCommandData::ObserveChatId data_4;
};

struct TgBotConnection {
    TgBotCommand cmd;
    union TgBotCommandUnion data;
};

using listener_callback_t = std::function<void(struct TgBotConnection)>;

void startListening(const listener_callback_t &cb);
void writeToSocket(const struct TgBotConnection &conn);
