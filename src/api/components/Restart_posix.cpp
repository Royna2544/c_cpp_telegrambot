#include <api/MessageExt.hpp>
#include <api/components/Restart.hpp>
#include <restartfmt_parser.hpp>
#include <api/components/OnAnyMessage.hpp>
#include <api/components/OnCallbackQuery.hpp>
#include <api/components/ModuleManagement.hpp>

TgBotApiImpl::RestartCommand::RestartCommand(TgBotApiImpl::Ptr api)
    : _api(api) {
    api->getEvents().onCommand("restart", [this](Message::Ptr message) {
        commandFunction(std::make_shared<MessageExt>(std::move(message)));
    });
}

void TgBotApiImpl::RestartCommand::commandFunction(MessageExt::Ptr message) {
    if (!_api->authorized(message, "restart", AuthContext::Flags::None)) {
        return;
    }

    // typical chatid:int32_max
    std::array<char, RestartFmt::MAX_KNOWN_LENGTH> restartBuf = {0};
    int count = 0;
    std::vector<char *> myEnviron;

    const auto status = RestartFmt::handleMessage(_api);
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
    auto *const argv = _api->_provider->cmdline->argv();
    auto *const exe = argv[0];

    // Stop threads
    DLOG(INFO) << "Stopping running threads...";
    _api->_provider->threads->destroy();

    DLOG(INFO) << "Unloading command modules...";
    _api->kModuleLoader.reset();

    // Shut down async threads
    DLOG(INFO) << "Stopping async threads...";
    _api->onAnyMessageImpl.reset();
    _api->onCallbackQueryImpl.reset();

    // Log the restart command and the arguments to be used to restart the bot
    LOG(INFO) << fmt::format("Restarting bot with exe: {}, addenv {}", exe,
                             restartBuf.data());

    _api->sendReplyMessage(
        message->message(),
        access(_api->_loader->at(
                   message->get_or<MessageAttrs::Locale>(Locale::Default)),
               Strings::RESTARTING_BOT));

    // Call exeve
    execve(exe, argv, myEnviron.data());
}