#include <sstream>
#include <cassert>

#include "TgBotSocket.h"

template <typename T, int N, typename... V>
std::array<T, sizeof...(V)> make_array(V &&...v) {
    static_assert(sizeof...(V) == N, "Must match declared size");
    return {{std::forward<V>(v)...}};
}

#define ENUM_STR(enum) std::make_pair(enum, #enum)
#define ARGUMENT_SIZE(enum, len) std::make_pair(enum, len)

const auto kTgBotCommandStrMap = make_array<ConstArrayElem<TgBotCommand, std::string>, CMD_MAX - 1>(
    ENUM_STR(CMD_WRITE_MSG_TO_CHAT_ID),
    ENUM_STR(CMD_CTRL_SPAMBLOCK),
    ENUM_STR(CMD_OBSERVE_CHAT_ID),
    ENUM_STR(CMD_SEND_FILE_TO_CHAT_ID)
);

const auto kTgBotCommandArgsCount = make_array<ConstArrayElem<TgBotCommand, int>, CMD_MAX - 1>(
    ARGUMENT_SIZE(CMD_WRITE_MSG_TO_CHAT_ID, 2),  // chatid, msg
    ARGUMENT_SIZE(CMD_CTRL_SPAMBLOCK, 1),        // policy
    ARGUMENT_SIZE(CMD_OBSERVE_CHAT_ID, 2),       // chatid, policy
    ARGUMENT_SIZE(CMD_SEND_FILE_TO_CHAT_ID, 3)   // chatid, type, filepath
);

std::string toStr(TgBotCommand cmd) {
    for (const auto &elem : kTgBotCommandStrMap) {
        if (elem.first == cmd) {
            return elem.second;
        }
    }
    assert(0);
    __builtin_unreachable();
    return {};
}

int toCount(TgBotCommand cmd) {
    for (const auto &elem : kTgBotCommandArgsCount) {
        if (elem.first == cmd) {
            return elem.second;
        }
    }
    assert(0);
    __builtin_unreachable();
    return 0;
}

std::string toHelpText(void) {
    std::stringstream help;
    for (const auto &ent : kTgBotCommandStrMap) {
        help << ent.second << ": value " << ent.first << ", Requires "
             << toCount(ent.first) << " argument(s)" << std::endl;
    }
    return help.str();
}
