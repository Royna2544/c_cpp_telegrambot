#pragma once

#include <BotReplyMessage.h>

#include <functional>

/**
 * command_callback_t - callback function for a generic command handler
 * Passes a Bot reference object and callback message pointer
 */
using command_callback_t = std::function<void(Bot&, const Message::Ptr&)>;

/**
 * @brief Adds a command to the bot.
 *
 * This function adds a command to the bot with the specified name and callback function.
 * The callback function is called when the bot receives a message containing the command.
 *
 * @param bot The bot reference object.
 * @param cmd The name of the command.
 * @param cb The callback function to be called when the command is received.
 * @param enforced A boolean value indicating whether the command is enforced or not.
 */
void bot_AddCommand(Bot& bot, const std::string& cmd, command_callback_t cb, bool enforced);


/**
 * @brief Removes a command from the bot.
 *
 * This function removes a command from the bot with the specified name.
 *
 * @param bot The bot reference object.
 * @param cmd The name of the command to be removed.
 */
void bot_RemoveCommand(Bot& bot, const std::string& cmd);
