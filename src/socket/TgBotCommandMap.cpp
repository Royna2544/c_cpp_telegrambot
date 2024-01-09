#include <cassert>
#include <mutex>
#include <sstream>

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
    ENUM_STR(CMD_SEND_FILE_TO_CHAT_ID),
    ENUM_STR(CMD_OBSERVE_ALL_CHATS)
);

const auto kTgBotCommandArgsCount = make_array<ConstArrayElem<TgBotCommand, int>, CMD_MAX - 1>(
    ARGUMENT_SIZE(CMD_WRITE_MSG_TO_CHAT_ID, 2),  // chatid, msg
    ARGUMENT_SIZE(CMD_CTRL_SPAMBLOCK, 1),        // policy
    ARGUMENT_SIZE(CMD_OBSERVE_CHAT_ID, 2),       // chatid, policy
    ARGUMENT_SIZE(CMD_SEND_FILE_TO_CHAT_ID, 3),  // chatid, type, filepath
    ARGUMENT_SIZE(CMD_OBSERVE_ALL_CHATS, 1)      // policy
);

std::string toStr(TgBotCommand cmd) {
    for (const auto &elem : kTgBotCommandStrMap) {
        if (elem.first == cmd) {
            return elem.second;
        }
    }
    assert(0);
    return {};
}

int toCount(TgBotCommand cmd) {
    for (const auto &elem : kTgBotCommandArgsCount) {
        if (elem.first == cmd) {
            return elem.second;
        }
    }
    assert(0);
    return 0;
}

std::string toHelpText(void) {
    static std::string helptext;
    static std::once_flag once;

    std::call_once(once, [] {
        std::stringstream help;
        for (const auto &ent : kTgBotCommandStrMap) {
            int count = toCount(ent.first);

            help << ent.second << ": value " << ent.first << ", Requires "
                 << count << " argument";
            if (count > 1)
                help << "s";
            help << std::endl;
        }
        helptext = help.str();
    });
    return helptext;
}
