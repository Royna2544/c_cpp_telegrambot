#include <BotReplyMessage.h>
#include <CommandModule.h>
#include <ConfigManager.h>
#include <absl/log/log.h>
#include <libos/libsighandler.h>
#include <unistd.h>

#include <TryParseStr.hpp>
#include <memory>

extern char **environ;
static void restartCommandFn(const Bot &bot, const Message::Ptr message) {
    std::array<char, 32> restartBuf = {0};
    int argc = 0;
    int count = 1;
    char *const *argv = nullptr;
    std::vector<char *> myEnviron;

    if (auto *const var = getenv("RESTART"); var != nullptr) {
        MessageId id;
        LOG(INFO) << "RESTART env var set to " << var;
        unsetenv("RESTART");
        installSignalHandler();

        if (try_parse(var, &id) && id == message->messageId) {
            LOG(INFO) << "Restart success! From messageId "
                      << message->messageId;
            bot_sendReplyMessage(bot, message, "Restart success!");
            return;
        }
    }
    for (; environ[count]; ++count);
    myEnviron.reserve(count + 2);
    memcpy(myEnviron.data(), environ, count * sizeof(char *));
    snprintf(restartBuf.data(), restartBuf.size(), "RESTART=%d",
             message->messageId);
    myEnviron[count] = restartBuf.data();
    myEnviron[count + 1] = nullptr;
    copyCommandLine(CommandLineOp::GET, &argc, &argv);
    LOG(INFO) << "Restarting bot with exe: " << argv[0] << ", addenv "
              << restartBuf.data();
    bot_sendReplyMessage(bot, message, "Restarting bot instance...");
    defaultCleanupFunction();
    execve(argv[0], argv, myEnviron.data());
}

void loadcmd_restart(CommandModule &module) {
    module.command = "restart";
    module.description = "Restarts the bot";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = restartCommandFn;
}