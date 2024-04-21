#include <EnumArrayHelpers.h>
#include <absl/log/log.h>

#include <mutex>
#include <sstream>

#include "TgBotSocket.h"

#define ARGUMENT_SIZE(enum, len) array_helpers::make_elem(enum, len)

constexpr auto kTgBotCommandStrMap =
    array_helpers::make<CMD_CLIENT_MAX, TgBotCommand, const char*>(
        ENUM_AND_STR(CMD_WRITE_MSG_TO_CHAT_ID),
        ENUM_AND_STR(CMD_CTRL_SPAMBLOCK), ENUM_AND_STR(CMD_OBSERVE_CHAT_ID),
        ENUM_AND_STR(CMD_SEND_FILE_TO_CHAT_ID),
        ENUM_AND_STR(CMD_OBSERVE_ALL_CHATS),
        ENUM_AND_STR(CMD_DELETE_CONTROLLER_BY_ID),
        ENUM_AND_STR(CMD_GET_UPTIME));

const auto kTgBotCommandArgsCount =
    array_helpers::make<CMD_CLIENT_MAX, TgBotCommand, int>(
        ARGUMENT_SIZE(CMD_WRITE_MSG_TO_CHAT_ID, 2),  // chatid, msg
        ARGUMENT_SIZE(CMD_CTRL_SPAMBLOCK, 1),        // policy
        ARGUMENT_SIZE(CMD_OBSERVE_CHAT_ID, 2),       // chatid, policy
        ARGUMENT_SIZE(CMD_SEND_FILE_TO_CHAT_ID, 3),  // chatid, type, filepath
        ARGUMENT_SIZE(CMD_OBSERVE_ALL_CHATS, 1),     // policy
        ARGUMENT_SIZE(CMD_DELETE_CONTROLLER_BY_ID, 1),  // id
        ARGUMENT_SIZE(CMD_GET_UPTIME, 0));

namespace TgBotCmd {
std::string toStr(TgBotCommand cmd) {
    const auto *const it = array_helpers::find(kTgBotCommandStrMap, cmd);
    LOG_IF(FATAL, it == kTgBotCommandStrMap.end())
        << "Couldn't find cmd " << cmd << " in map";
    return it->second;
}

int toCount(TgBotCommand cmd) {
    const auto* const it = array_helpers::find(kTgBotCommandArgsCount, cmd);
    LOG_IF(FATAL, it == kTgBotCommandArgsCount.end())
        << "Couldn't find cmd " << cmd << " in map";
    return it->second;
}

bool isClientCommand(TgBotCommand cmd) { return cmd < CMD_CLIENT_MAX; }

bool isInternalCommand(TgBotCommand cmd) {
    return cmd >= CMD_SERVER_INTERNAL_START;
}

std::string getHelpText() {
    static std::string helptext;
    static std::once_flag once;

    std::call_once(once, [] {
        std::stringstream help;
        for (const auto& ent : kTgBotCommandStrMap) {
            int count = 0;

            if (ent.first == CMD_EXIT) {
                continue;
            }
            count = TgBotCmd::toCount(ent.first);

            help << ent.second << ": value " << ent.first << ", Requires "
                 << count << " argument";
            if (count > 1) {
                help << "s";
            }
            help << std::endl;
        }
        helptext = help.str();
    });
    return helptext;
}
}  // namespace TgBotCmd
