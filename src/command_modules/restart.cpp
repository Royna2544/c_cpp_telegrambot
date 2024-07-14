
#include <ConfigManager.h>
#include <absl/log/log.h>
#include <unistd.h>

#include <TgBotWrapper.hpp>
#include <TryParseStr.hpp>
#include <cinttypes>
#include <libos/libsighandler.hpp>

extern char **environ;

static void restartCommandFn(const TgBotWrapper *tgBotWrapper,
                             MessagePtr message) {
    std::array<char, sizeof("RESTART=999999999999")> restartBuf = {0};
    int argc = 0;
    int count = 0;
    char *const *argv = nullptr;
    std::vector<char *> myEnviron;

    if (std::string u; ConfigManager::getEnv("RESTART", u)) {
        snprintf(restartBuf.data(), restartBuf.size(), "RESTART=%" PRId64 ":%d",
                 message->chat->id, message->messageId);
        if (u == restartBuf.data()) {
            LOG(INFO) << "Ignoring restart command";
            return;
        }
    }

    // Get the size of environment buffer
    for (; environ[count]; ++count);
    // Get ready to insert 1 more to the environment buffer
    myEnviron.resize(count + 2);
    // Copy the environment buffer to the new buffer
    memcpy(myEnviron.data(), environ, count * sizeof(char *));
    // Add the restart command env to the environment buffer
    snprintf(restartBuf.data(), restartBuf.size(), "RESTART=%" PRId64 ":%d",
             message->chat->id, message->messageId);
    myEnviron[count] = restartBuf.data();
    myEnviron[count + 1] = nullptr;
    // Copy the command line used to launch the bot
    copyCommandLine(CommandLineOp::GET, &argc, &argv);
    LOG(INFO) << "Restarting bot with exe: " << argv[0] << ", addenv "
              << restartBuf.data();
    tgBotWrapper->sendReplyMessage(message, "Restarting bot instance...");
    // Call exeve
    execve(argv[0], argv, myEnviron.data());
}

DYN_COMMAND_FN(n, module) {
    module.command = "restart";
    module.description = "Restarts the bot";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = restartCommandFn;
    return true;
}