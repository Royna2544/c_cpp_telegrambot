#include <Authorization.h>
#include <BotAddCommand.h>

void bot_AddCommandPermissive(Bot& bot, const char* cmd, command_callback_t cb) {
    auto authFn = [&, cb](const Message::Ptr message) {
        if (!Authorized(message, true, true)) return;
        cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}

void bot_AddCommandEnforced(Bot& bot, const char* cmd, command_callback_t cb) {
    auto authFn = [&, cb](const Message::Ptr message) {
        if (!Authorized(message)) return;
        cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}

void bot_AddCommandEnforcedCompiler(Bot& bot, const char* cmd,
                                    ProgrammingLangs lang, command_callback_compiler_t cb) {
    auto compiler = findCompiler(lang);
    if (!compiler.empty()) {
        bot_AddCommandEnforced(bot, cmd, std::bind(cb, pholder1, pholder2, compiler));
    } else {
        LOG_W("Unsupported cmd '%s' (compiler)", cmd);
    }
}
