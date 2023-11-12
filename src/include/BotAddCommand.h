#pragma once

#include <BotReplyMessage.h>
#include <NamespaceImport.h>

#include <functional>

#include "utils/libutils.h"

using command_callback_t = std::function<void(const Bot&, const Message::Ptr&)>;

void bot_AddCommandPermissive(Bot& bot, const char* cmd, command_callback_t cb);
void bot_AddCommandEnforced(Bot& bot, const char* cmd, command_callback_t cb);
static inline void bot_AddCommandEnforcedCompiler(Bot& bot, const char* cmd, std::string compiler, command_callback_t cb) {
    if (!compiler.empty()) {
        bot_AddCommandEnforced(bot, cmd, cb);
    } else {
        LOG_W("Unsupported cmd '%s' (compiler)", cmd);
    }
}
