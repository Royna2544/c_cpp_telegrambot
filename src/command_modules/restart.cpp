
#include <ConfigManager.h>
#include <absl/log/log.h>
#include <unistd.h>

#include <CommandLine.hpp>
#include <TgBotWrapper.hpp>
#include <TryParseStr.hpp>
#include <cinttypes>
#include <cstdlib>
#include <libos/libsighandler.hpp>
#include <restartfmt_parser.hpp>

extern char **environ;

DECLARE_COMMAND_HANDLER(restart, tgBotWrapper, message) {
    // typical chatid:int32_max
    std::array<char, RestartFmt::MAX_KNOWN_LENGTH> restartBuf = {0};
    int count = 0;
    std::vector<char *> myEnviron;

    if (auto status = RestartFmt::handleMessage(tgBotWrapper); !status.ok()) {
        LOG(ERROR) << "Failed to handle restart message: " << status;
    }

    // Get the size of environment buffer
    for (; environ[count] != nullptr; ++count);
    // Get ready to insert 1 more to the environment buffer
    myEnviron.resize(count + 2);
    // Copy the environment buffer to the new buffer
    memcpy(myEnviron.data(), environ, count * sizeof(char *));

    // Set the restart command env to the environment buffer
    const auto restartEnv =
        RestartFmt::toString({message->chat->id, message->messageId}, true);
    strncpy(restartBuf.data(), restartEnv.c_str(), restartEnv.size());

    // Append the restart env to the environment buffer
    myEnviron[count] = restartBuf.data();
    myEnviron[count + 1] = nullptr;

    // Copy the command line used to launch the bot
    auto *const argv = CommandLine::getInstance()->getArgv();
    auto *const exe = argv[0];

    // Log the restart command and the arguments to be used to restart the bot
    LOG(INFO) << "Restarting bot with exe: " << exe << ", addenv "
              << restartBuf.data();
    tgBotWrapper->sendReplyMessage(message, "Restarting bot instance...");

    // Call exeve
    execve(exe, argv, myEnviron.data());
}

DYN_COMMAND_FN(n, module) {
    module.command = "restart";
    module.description = "Restarts the bot";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = COMMAND_HANDLER_NAME(restart);
    return true;
}