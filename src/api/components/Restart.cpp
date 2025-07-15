#include <absl/log/check.h>

#include <Env.hpp>
#include <api/MessageExt.hpp>
#include <api/components/ModuleManagement.hpp>
#include <api/components/OnAnyMessage.hpp>
#include <api/components/OnCallbackQuery.hpp>
#include <api/components/Restart.hpp>
#include <iomanip>
#include <restartfmt_parser.hpp>

#ifdef __APPLE__
extern char **environ;
#endif  // __APPLE__

TgBotApiImpl::RestartCommand::RestartCommand(TgBotApiImpl::Ptr api)
    : _api(api) {
    api->getEvents().onCommand("restart", [this](Message::Ptr message) {
        commandFunction(std::make_unique<MessageExt>(std::move(message)).get());
    });
}

void TgBotApiImpl::RestartCommand::commandFunction(MessageExt::Ptr message) {
    if (!_api->authorized(message, "restart", AuthContext::AccessLevel::AdminUser)) {
        return;
    }
    if (!_api->isMyCommand(message)) {
        return;
    }
    if (RestartFmt::isRestartedByThisMessage(message)) {
        DLOG(INFO) << "Avoiding restart loop";
        return;
    }

    // typical chatid:int32_max
    int count = 0;
    std::vector<char *> myEnviron;

    // Get the size of environment buffer
    for (; environ[count] != nullptr; ++count);
    // Get ready to insert 1 more to the environment buffer
    myEnviron.resize(count + 2);
    // Copy the environment buffer to the new buffer
    memcpy(myEnviron.data(), environ, count * sizeof(char *));
    // Check if the environ RESTART exists already
    auto [be, en] = std::ranges::remove_if(myEnviron, [](const char *env) {
        return env != nullptr
                   ? std::string_view(env).starts_with(RestartFmt::ENV_VAR_NAME)
                   : false;
    });
    // If the environ RESTART exists, remove it
    if (std::distance(be, en) != 0) {
        DLOG(INFO) << "Removing existing RESTART=";
        myEnviron.erase(be, en);
    }

    std::array<char, RestartFmt::MAX_KNOWN_LENGTH> restartBuf = {0};
    std::string restartEnv =
        fmt::format("{}={}", RestartFmt::ENV_VAR_NAME,
                    RestartFmt::Type{message->message()}.to_string());
    CHECK(restartEnv.size() < RestartFmt::MAX_KNOWN_LENGTH)
        << "RestartFmt::MAX_KNOWN_LENGTH is violated: for string "
        << std::quoted(restartEnv);
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
        _api->_loader->at(message->get<MessageAttrs::Locale>())
            ->get(Strings::RESTARTING_BOT));

    execve(argv[0], argv, myEnviron.data());
}
