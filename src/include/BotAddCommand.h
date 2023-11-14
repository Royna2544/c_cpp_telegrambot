#pragma once

#include <BotReplyMessage.h>
#include <NamespaceImport.h>

#include <functional>

#include "utils/libutils.h"

using command_callback_t = std::function<void(const Bot&, const Message::Ptr&)>;

void bot_AddCommandPermissive(Bot& bot, const char* cmd, command_callback_t cb);
void bot_AddCommandEnforced(Bot& bot, const char* cmd, command_callback_t cb);

using command_callback_compiler_t = std::function<void(const Bot&, const Message::Ptr&,
                                                       const std::string& compiler)>;

void bot_AddCommandEnforcedCompiler(Bot& bot, const char* cmd, ProgrammingLangs lang,
                                    command_callback_compiler_t cb);
