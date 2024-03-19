#include <ConfigManager.h>
#include <unistd.h>

#include "BotReplyMessage.h"
#include "CommandModule.h"
#include "Logging.h"

extern char **environ;
static void restartCommandFn(const Bot &bot, const Message::Ptr message) {
    int argc, count = 1;
    char *const *argv, **myEnviron, restartBuf[32] = {0};

    if (const auto var = getenv("RESTART"); var != nullptr) {
        LOG(LogLevel::DEBUG, "RESTART env var set to %s", var);
        unsetenv("RESTART");
        if (std::stoi(var) == message->messageId) {
            LOG(LogLevel::DEBUG, "Restart success! From messageId %d", message->messageId);
            bot_sendReplyMessage(bot, message, "Restart success!");
            return;
        }
    }
    for (;environ[count]; ++count);
    myEnviron = new char *[count + 2];
    memcpy(myEnviron, environ, count * sizeof(char *));
    snprintf(restartBuf, sizeof(restartBuf), "RESTART=%d", message->messageId);
    myEnviron[count] = restartBuf;
    myEnviron[count + 1] = 0;
    copyCommandLine(CommandLineOp::GET, &argc, &argv);
    LOG(LogLevel::DEBUG, "Restarting bot with command line: %s, addenv %s",
        argv[0], restartBuf);
    bot_sendReplyMessage(bot, message, "Restarting bot instance...");
    execve(argv[0], argv, myEnviron);
}

struct CommandModule cmd_restart("restart", "Restarts the bot",
                                 CommandModule::Flags::Enforced,
                                 restartCommandFn);
