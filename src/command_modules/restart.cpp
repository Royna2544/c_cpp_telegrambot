#include <absl/log/log.h>
#include <fmt/format.h>
#include <unistd.h>

#include <TryParseStr.hpp>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <cstdlib>
#include <libos/libsighandler.hpp>
#include <restartfmt_parser.hpp>

#include "StringResLoader.hpp"
#include "api/MessageExt.hpp"

extern char **environ;  // NOLINT

DECLARE_COMMAND_HANDLER(restart) {
    // typical chatid:int32_max
    std::array<char, RestartFmt::MAX_KNOWN_LENGTH> restartBuf = {0};
    int count = 0;
    std::vector<char *> myEnviron;

    const auto status = RestartFmt::handleMessage(api);
    LOG_IF(ERROR, status.code() == absl::StatusCode::kInvalidArgument)
        << "Failed to handle restart message: " << status;
    if (status.ok()) {
        return;
    }

    // Get the size of environment buffer
    for (; environ[count] != nullptr; ++count);
    // Get ready to insert 1 more to the environment buffer
    myEnviron.resize(count + 2);
    // Copy the environment buffer to the new buffer
    memcpy(myEnviron.data(), environ, count * sizeof(char *));

    // Set the restart command env to the environment buffer
    const auto restartEnv =
        RestartFmt::toString({message->get<MessageAttrs::Chat>()->id,
                              message->get<MessageAttrs::MessageId>()},
                             true);
    strncpy(restartBuf.data(), restartEnv.c_str(), restartEnv.size());

    // Append the restart env to the environment buffer
    myEnviron[count] = restartBuf.data();
    myEnviron[count + 1] = nullptr;

    // Copy the command line used to launch the bot
    auto *const argv = provider->cmdline->argv();
    auto *const exe = argv[0];

    // Log the restart command and the arguments to be used to restart the bot
    LOG(INFO) << fmt::format("Restarting bot with exe: {}, addenv {}", exe,
                             restartBuf.data());
    api->sendReplyMessage(message->message(),
                          access(res, Strings::RESTARTING_BOT));

    // Call exeve
    execve(exe, argv, myEnviron.data());
}

DYN_COMMAND_FN(n, module) {
    module.name = "restart";
    module.description = "Restarts the bot";
    module.flags = CommandModule::Flags::Enforced;
    module.function = COMMAND_HANDLER_NAME(restart);
    return true;
}