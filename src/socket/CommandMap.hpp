#pragma once

#include <fmt/format.h>

#include <string>

#include <SocketExports.h>
#include "ApiDef.hpp"

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
            DEFINE_STR(CMD_GET_UPTIME);
            DEFINE_STR(CMD_TRANSFER_FILE);
            DEFINE_STR(CMD_TRANSFER_FILE_REQUEST);
            DEFINE_STR(CMD_GET_UPTIME_CALLBACK);
            DEFINE_STR(CMD_GENERIC_ACK);
            DEFINE_STR(CMD_OPEN_SESSION);
            DEFINE_STR(CMD_OPEN_SESSION_ACK);
            DEFINE_STR(CMD_CLOSE_SESSION);
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
int Socket_API toCount(Command cmd);

/**
 * @brief Check if given command is a client command
 *
 * @param cmd Command to check
 * @return true if given command is a client command, false otherwise
 */
bool Socket_API isClientCommand(Command cmd);

/**
 * @brief Check if given command is an internal command
 *
 * @param cmd Command to check
 * @return true if given command is an internal command, false otherwise
 */
bool Socket_API isInternalCommand(Command cmd);

/**
 * @brief Get help text for Command
 *
 * @return std::string help text for Command
 */
std::string Socket_API getHelpText(void);
}  // namespace TgBotSocket::CommandHelpers
