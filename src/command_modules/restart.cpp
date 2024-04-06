#include <ConfigManager.h>
#include <absl/log/log.h>
#include <unistd.h>
#include <memory>

#include "BotReplyMessage.h"
#include "CommandModule.h"
#include "libos/libsighandler.h"

extern char **environ;
static void restartCommandFn(const Bot &bot, const Message::Ptr message) {
    std::array<char, 32> restartBuf = {0};
    char **myEnviron;
    int argc = 0, count = 1;
    char *const *argv = nullptr;

    if (const auto var = getenv("RESTART"); var != nullptr) {
        LOG(INFO) << "RESTART env var set to " << var;
        unsetenv("RESTART");
        installSignalHandler();
        if (std::stoi(var) == message->messageId) {
            LOG(INFO) << "Restart success! From messageId "
                      << message->messageId;
            bot_sendReplyMessage(bot, message, "Restart success!");
            return;
        }
    }
    for (; environ[count]; ++count)
        ;
    myEnviron = new char *[count + 2];
    memcpy(myEnviron, environ, count * sizeof(char *));
    snprintf(restartBuf.data(), restartBuf.size(), "RESTART=%d", message->messageId);
    myEnviron[count] = restartBuf.data();
    myEnviron[count + 1] = 0;
    copyCommandLine(CommandLineOp::GET, &argc, &argv);
    LOG(INFO) << "Restarting bot with exe: " << argv[0] << ", addenv "
              << restartBuf.data();
    bot_sendReplyMessage(bot, message, "Restarting bot instance...");
    defaultCleanupFunction();
    execve(argv[0], argv, myEnviron);
    delete[] myEnviron;
}

struct CommandModule cmd_restart("restart", "Restarts the bot",
                                 CommandModule::Flags::Enforced,
                                 restartCommandFn);
