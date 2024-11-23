#include "TgBotCommandMap.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <_TgBotSocketCommands.hpp>
#include <mutex>

namespace TgBotSocket::CommandHelpers {

struct CommandEntry {
    TgBotSocket::Command cmd;
    int argCount;
    std::string_view argHelp;
};

constexpr std::array<CommandEntry, static_cast<int>(Command::CMD_CLIENT_MAX)>
    kCommandArray{
        CommandEntry{Command::CMD_WRITE_MSG_TO_CHAT_ID, 2, "Chat ID, Message"},
        CommandEntry{Command::CMD_CTRL_SPAMBLOCK, 1, "SpamBlockCtrl enum"},
        CommandEntry{Command::CMD_OBSERVE_CHAT_ID, 2,
                     "Chat ID, Observe_or_not"},
        CommandEntry{Command::CMD_SEND_FILE_TO_CHAT_ID, 3,
                     "Chat ID, FileType enum, FilePath"},
        CommandEntry{Command::CMD_OBSERVE_ALL_CHATS, 1, "Observe_or_not"},
        CommandEntry{Command::CMD_DELETE_CONTROLLER_BY_ID, 1,
                     "ThreadManager::Usage enum, now no-op"},
        CommandEntry{Command::CMD_GET_UPTIME, 0, ""},
        CommandEntry{Command::CMD_UPLOAD_FILE, 2,
                     "Source File (In local), Dest File (In remote)"},
        CommandEntry{Command::CMD_DOWNLOAD_FILE, 2,
                     "Source File (In remote), Dest File (In local)"},
    };

int toCount(Command cmd) {
    const auto it = std::ranges::find_if(
        kCommandArray,
        [cmd](const CommandEntry& ent) { return ent.cmd == cmd; });
    DCHECK(it != kCommandArray.end());
    return (it != kCommandArray.end()) ? it->argCount : 0;
}

bool isClientCommand(Command cmd) { return cmd < Command::CMD_CLIENT_MAX; }

bool isInternalCommand(Command cmd) {
    return cmd >= Command::CMD_SERVER_INTERNAL_START;
}

std::string getHelpText() {
    static std::string helptext;
    static std::once_flag once;

    std::call_once(once, [] {
        std::vector<std::string> help;
        help.reserve(kCommandArray.size());
        for (const auto& ent : kCommandArray) {
            if (ent.argCount == 0) {
                help.emplace_back(fmt::format("{}: value {}, No arguments",
                                              ent.cmd,
                                              static_cast<int>(ent.cmd)));
            } else {
                help.emplace_back(fmt::format(
                    "{}: value {}, Arguments({}): {}", ent.cmd,
                    static_cast<int>(ent.cmd), ent.argCount, ent.argHelp));
            }
        }
        helptext = fmt::format("{}\n", fmt::join(help, "\n"));
    });
    return helptext;
}

}  // namespace TgBotSocket::CommandHelpers
