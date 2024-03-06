#include <Logging.h>
#include <EnumArrayHelpers.h>

#include <mutex>
#include <sstream>

#include "TgBotSocket.h"

#define ARGUMENT_SIZE(enum, len) array_helpers::make_elem(enum, len)

constexpr auto kTgBotCommandStrMap = array_helpers::make<CMD_MAX, TgBotCommand, const char *>(
    ENUM_AND_STR(CMD_EXIT),
    ENUM_AND_STR(CMD_WRITE_MSG_TO_CHAT_ID),
    ENUM_AND_STR(CMD_CTRL_SPAMBLOCK),
    ENUM_AND_STR(CMD_OBSERVE_CHAT_ID),
    ENUM_AND_STR(CMD_SEND_FILE_TO_CHAT_ID),
    ENUM_AND_STR(CMD_OBSERVE_ALL_CHATS)
);

const auto kTgBotCommandArgsCount =  array_helpers::make<CMD_MAX - 1, TgBotCommand, int>(
    ARGUMENT_SIZE(CMD_WRITE_MSG_TO_CHAT_ID, 2),  // chatid, msg
    ARGUMENT_SIZE(CMD_CTRL_SPAMBLOCK, 1),        // policy
    ARGUMENT_SIZE(CMD_OBSERVE_CHAT_ID, 2),       // chatid, policy
    ARGUMENT_SIZE(CMD_SEND_FILE_TO_CHAT_ID, 3),  // chatid, type, filepath
    ARGUMENT_SIZE(CMD_OBSERVE_ALL_CHATS, 1)      // policy
);

namespace TgBotCmd {
std::string toStr(TgBotCommand cmd) {
    const auto it = array_helpers::find(kTgBotCommandStrMap, cmd);
    ASSERT(it != kTgBotCommandStrMap.end(), "Couldn't find cmd %d in map", cmd);
    return it->second;
}

int toCount(TgBotCommand cmd) {
    const auto it = array_helpers::find(kTgBotCommandArgsCount, cmd);
    ASSERT(it != kTgBotCommandArgsCount.end(), "Couldn't find cmd %d in map", cmd);
    return it->second;
}

std::string getHelpText(void) {
    static std::string helptext;
    static std::once_flag once;

    std::call_once(once, [] {
        std::stringstream help;
        for (const auto& ent : kTgBotCommandStrMap) {
            int count;

            if (ent.first == CMD_EXIT) continue;
            count = TgBotCmd::toCount(ent.first);

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
}
