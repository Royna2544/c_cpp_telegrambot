#include <Authorization.h>
#include <BotAddCommand.h>

static void NoCompilerCommandStub(const Bot& bot, const Message::Ptr& message) {
    bot_sendReplyMessage(bot, message, "Not supported in current host");
}

void bot_AddCommandPermissive(Bot& bot, const char* cmd, command_callback_t cb) {
    auto authFn = [&, cb](const Message::Ptr message) {
        if (Authorized(message, AuthorizeFlags::PERMISSIVE | AuthorizeFlags::REQUIRE_USER))
            cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}

void bot_AddCommandEnforced(Bot& bot, const char* cmd, command_callback_t cb) {
    auto authFn = [&, cb](const Message::Ptr message) {
        if (Authorized(message, AuthorizeFlags::REQUIRE_USER))
            cb(bot, message);
    };
    bot.getEvents().onCommand(cmd, authFn);
}

void bot_AddCommandEnforcedCompiler(Bot& bot, const char* cmd,
                                    ProgrammingLangs lang, command_callback_compiler_t cb) {
    std::string compiler;
    if (findCompiler(lang, compiler)) {
        bot_AddCommandEnforced(bot, cmd, std::bind(cb, std::placeholders::_1, std::placeholders::_2, compiler));
    } else {
        LOG_W("Unsupported cmd '%s' (compiler)", cmd);
        bot_AddCommandEnforced(bot, cmd, NoCompilerCommandStub);
    }
}
