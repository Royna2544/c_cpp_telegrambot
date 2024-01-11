#pragma once

#include <BotReplyMessage.h>
#include <CompilerInTelegram.h>
#include <Logging.h>
#include <NamespaceImport.h>

#include <functional>

/**
 * command_callback_t - callback function for a generic command handler
 * Passes a Bot reference object and callback message pointer
 */
using command_callback_t = std::function<void(const Bot&, const Message::Ptr&)>;

/**
 * bot_AddCommandPermissive - Add a bot command (permissive command)
 *
 * @param bot Bot object
 * @param cmd The command name in string
 * @param cb callback invoked on messages with matching cmd
 * @see Authorization.h
 */
void bot_AddCommandPermissive(Bot& bot, const char* cmd, command_callback_t cb);

/**
 * bot_AddCommandEnforced - Add a bot command (enforced command)
 *
 * @param bot Bot object
 * @param cmd The command name in string
 * @param cb callback invoked on messages with matching cmd
 * @see Authorization.h
 */
void bot_AddCommandEnforced(Bot& bot, const char* cmd, command_callback_t cb);

/**
 * command_callback_compiler_t - callback function for a compiler related command handler
 * Passes a Bot reference object and callback message pointer, and additionally compiler
 * exe path as string
 */
using command_callback_compiler_t = std::function<void(const Bot&, const Message::Ptr&,
                                                       const std::string& compiler)>;
/**
 * bot_AddCommandEnforcedCompiler - Add a bot command specialized for compiler
 *
 * @param bot Bot object
 * @param cmd The command name in string
 * @param lang The compiler type, will be passed to findCompiler() function in libutils.cpp
 * @param cb callback invoked on messages with matching cmd
 * @see Authorization.h
 */
void bot_AddCommandEnforcedCompiler(Bot& bot, const char* cmd, ProgrammingLangs lang,
                                    command_callback_compiler_t cb);
