#include <EnumArrayHelpers.h>
#include <absl/log/log.h>

#include <mutex>
#include <sstream>

#include <TgBotSocket_Export.hpp>

#define ARGUMENT_SIZE(enum, len) array_helpers::make_elem(Command::enum, len)

#undef ENUM_AND_STR
#define ENUM_AND_STR(e) \
    array_helpers::make_elem<Command, const char*>(Command::e, #e)

using namespace TgBotSocket;

constexpr int MAX_LEN = static_cast<int>(Command::CMD_CLIENT_MAX);

constexpr auto kTgBotCommandStrMap =
    array_helpers::make<MAX_LEN, TgBotSocket::Command, const char*>(
        ENUM_AND_STR(CMD_WRITE_MSG_TO_CHAT_ID),
        ENUM_AND_STR(CMD_CTRL_SPAMBLOCK), ENUM_AND_STR(CMD_OBSERVE_CHAT_ID),
        ENUM_AND_STR(CMD_SEND_FILE_TO_CHAT_ID),
        ENUM_AND_STR(CMD_OBSERVE_ALL_CHATS),
        ENUM_AND_STR(CMD_DELETE_CONTROLLER_BY_ID), ENUM_AND_STR(CMD_GET_UPTIME),
        ENUM_AND_STR(CMD_UPLOAD_FILE), ENUM_AND_STR(CMD_DOWNLOAD_FILE),
        ENUM_AND_STR(CMD_UPLOAD_FILE_DRY));

const auto kTgBotCommandArgsCount =
    array_helpers::make<MAX_LEN - 1, TgBotSocket::Command, int>(
        ARGUMENT_SIZE(CMD_WRITE_MSG_TO_CHAT_ID, 2),  // chatid, msg
        ARGUMENT_SIZE(CMD_CTRL_SPAMBLOCK, 1),        // policy
        ARGUMENT_SIZE(CMD_OBSERVE_CHAT_ID, 2),       // chatid, policy
        ARGUMENT_SIZE(CMD_SEND_FILE_TO_CHAT_ID, 3),  // chatid, type, filepath
        ARGUMENT_SIZE(CMD_OBSERVE_ALL_CHATS, 1),     // policy
        ARGUMENT_SIZE(CMD_DELETE_CONTROLLER_BY_ID, 1),  // id
        ARGUMENT_SIZE(CMD_GET_UPTIME, 0),
        ARGUMENT_SIZE(CMD_UPLOAD_FILE, 2),     // source, dest filepath
        ARGUMENT_SIZE(CMD_DOWNLOAD_FILE, 2));  // source, dest filepath

namespace TgBotSocket::CommandHelpers {

std::string toStr(Command cmd) {
    const auto* const it = array_helpers::find(kTgBotCommandStrMap, cmd);
    LOG_IF(FATAL, it == kTgBotCommandStrMap.end())
        << "Couldn't find cmd " << static_cast<int>(cmd) << " in map";
    return it->second;
}

int toCount(Command cmd) {
    const auto* const it = array_helpers::find(kTgBotCommandArgsCount, cmd);
    LOG_IF(FATAL, it == kTgBotCommandArgsCount.end())
        << "Couldn't find cmd " << static_cast<int>(cmd) << " in map";
    return it->second;
}

bool isClientCommand(Command cmd) { return cmd < Command::CMD_CLIENT_MAX; }

bool isInternalCommand(Command cmd) {
    return cmd >= Command::CMD_SERVER_INTERNAL_START;
}

std::string getHelpText() {
    static std::string helptext;
    static std::once_flag once;

    std::call_once(once, [] {
        std::stringstream help;
        for (const auto& ent : kTgBotCommandStrMap) {
            int count = 0;

            count = toCount(ent.first);

            help << ent.second << ": value " << static_cast<int>(ent.first)
                 << ", Requires " << count << " argument";
            if (count > 1) {
                help << "s";
            }
            help << std::endl;
        }
        helptext = help.str();
    });
    return helptext;
}

}  // namespace TgBotSocket::CommandHelpers
