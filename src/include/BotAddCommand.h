#pragma once

#include <BotReplyMessage.h>
#include <Logging.h>

#include <functional>

/**
 * command_callback_t - callback function for a generic command handler
 * Passes a Bot reference object and callback message pointer
 */
using command_callback_t = std::function<void(Bot&, const Message::Ptr&)>;

/**
 * bot_AddCommandPermissive - Add a bot command (permissive command)
 *
 * @param bot Bot object
 * @param cmd The command name in string
 * @param cb callback invoked on messages with matching cmd
 * @see Authorization.h
 */
void bot_AddCommandPermissive(Bot& bot, const std::string& cmd, command_callback_t cb);

/**
 * bot_AddCommandEnforced - Add a bot command (enforced command)
 *
 * @param bot Bot object
 * @param cmd The command name in string
 * @param cb callback invoked on messages with matching cmd
 * @see Authorization.h
 */
void bot_AddCommandEnforced(Bot& bot, const std::string& cmd, command_callback_t cb);
