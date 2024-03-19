#include <ConfigManager.h>
#include <unistd.h>

#include "BotReplyMessage.h"
#include "CommandModule.h"

extern char **environ;
static void restartCommandFn(const Bot &bot, const Message::Ptr message) {
    int argc;
    char *const *argv;
    copyCommandLine(CommandLineOp::GET, &argc, &argv);
    bot_sendReplyMessage(bot, message, "Restarting bot instance...");
    execve(argv[0], argv, environ);
}

struct CommandModule cmd_restart("restart", "Restarts the bot",
                                 CommandModule::Flags::Enforced,
                                 restartCommandFn);