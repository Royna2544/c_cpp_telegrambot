#pragma once

#include <fmt/format.h>

#include <string>

#include "include/TgBotSocket_Export.hpp"

template <>
struct fmt::formatter<TgBotSocket::Command> : formatter<std::string_view> {
    using Command = TgBotSocket::Command;

    // parse is inherited from formatter<string_view>.
    auto format(TgBotSocket::Command c,
                format_context& ctx) const -> format_context::iterator {
        string_view name = "unknown";
        switch (c) {
#define DEFINE_STR(x) \
    case Command::x:  \
        name = #x;    \
        break
            DEFINE_STR(CMD_WRITE_MSG_TO_CHAT_ID);
            DEFINE_STR(CMD_CTRL_SPAMBLOCK);
            DEFINE_STR(CMD_OBSERVE_CHAT_ID);
            DEFINE_STR(CMD_SEND_FILE_TO_CHAT_ID);
            DEFINE_STR(CMD_OBSERVE_ALL_CHATS);
            DEFINE_STR(CMD_DELETE_CONTROLLER_BY_ID);
            DEFINE_STR(CMD_GET_UPTIME);
            DEFINE_STR(CMD_UPLOAD_FILE);
            DEFINE_STR(CMD_DOWNLOAD_FILE);
            DEFINE_STR(CMD_CLIENT_MAX);
            DEFINE_STR(CMD_SERVER_INTERNAL_START);
            DEFINE_STR(CMD_GENERIC_ACK);
            DEFINE_STR(CMD_UPLOAD_FILE_DRY);
            DEFINE_STR(CMD_UPLOAD_FILE_DRY_CALLBACK);
            DEFINE_STR(CMD_DOWNLOAD_FILE_CALLBACK);
#undef DEFINE_STR
            default:
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

namespace TgBotSocket::CommandHelpers {

/**
 * @brief Get count of Command
 *
 * @param cmd Command to get arg count of
 * @return required arg count of Command
 */
int toCount(Command cmd);

/**
 * @brief Check if given command is a client command
 *
 * @param cmd Command to check
 * @return true if given command is a client command, false otherwise
 */
bool isClientCommand(Command cmd);

/**
 * @brief Check if given command is an internal command
 *
 * @param cmd Command to check
 * @return true if given command is an internal command, false otherwise
 */
bool isInternalCommand(Command cmd);

/**
 * @brief Get help text for Command
 *
 * @return std::string help text for Command
 */
std::string getHelpText(void);
}  // namespace TgBotSocket::CommandHelpers
