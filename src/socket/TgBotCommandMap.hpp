#pragma once

#include <string>
#include "include/TgBotSocket_Export.hpp"

namespace TgBotSocket::CommandHelpers {
/**
 * @brief Convert TgBotCommand to string
 *
 * @param cmd Command to convert
 * @return std::string string representation of Command enum
 */
std::string toStr(Command cmd);

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
} // namespace TgBotSocket::CommandHelpers

