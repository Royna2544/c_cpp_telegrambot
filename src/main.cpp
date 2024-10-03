#include <Authorization.h>
#include <ConfigManager.h>
#include <ResourceManager.h>
#include <absl/log/log.h>
#include <absl/log/log_sink_registry.h>

#include <AbslLogInit.hpp>
#include <CommandLine.hpp>
#include <DurationPoint.hpp>
#include <InitTask.hpp>
#include <LogSinks.hpp>
#include <ManagedThreads.hpp>
#include <Random.hpp>
#include <RegEXHandler.hpp>
#include <SpamBlock.hpp>
#include <StringResManager.hpp>
#include <TgBotWebpage.hpp>
#include <TgBotWrapper.hpp>
#include <TryParseStr.hpp>
#include <boost/system/system_error.hpp>
#include <cstdint>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <filesystem>
#include <libos/libfs.hpp>
#include <libos/libsighandler.hpp>
#include <memory>
#include <ml/ChatDataCollector.hpp>
#include <utility>

#ifndef WINDOWS_BUILD
#include <restartfmt_parser.hpp>
#endif

#ifdef SOCKET_CONNECTION
#include <ChatObserver.h>

#include <logging/LoggingServer.hpp>
#include <socket/interface/impl/bot/TgBotSocketInterface.hpp>
#endif

#include <tgbot/tgbot.h>

template <typename T>
void init(InitTask& task) = delete;

template <>
void init<StringResManager>(InitTask& task) {
    task << "Load string resources";
    auto locale = getVariable(ConfigManager::Configs::LOCALE);
    if (!locale) {
        LOG(WARNING) << "Using default locale: en-US";
        locale = "en-US";
    }
    auto loader = std::make_unique<StringResLoader>();
    bool res = loader->parse(FS::getPathForType(FS::PathType::RESOURCES) /
                                 "strings" / locale->append(".xml"),
                             STRINGRES_MAX);
    if (!res) {
        LOG(ERROR) << "Failed to parse string res";
        task << InitFailed;
    } else {
        StringResManager::initInstance(std::move(loader));
        task << InitSuccess;
    }
}

template <>
void init<TgBotWrapper>(InitTask& task) {
    task << "Load command modules";
    const auto& bot = TgBotWrapper::getInstance();

    // Load modules
    std::filesystem::path modules_path =
        FS::getPathForType(FS::PathType::MODULES_INSTALLED);
    LOG(INFO) << "Loading commands from " << modules_path.string();
    for (std::filesystem::directory_iterator it(modules_path);
         it != std::filesystem::directory_iterator(); ++it) {
        if (it->path().filename().string().starts_with("libcmd_")) {
            auto module = std::make_unique<CommandModule>(*it);
            bot->addCommand(std::move(module));
        }
    }
    task << InitSuccess;

    task << "Updating bot command list";
    bool success = false;
    try {
        success = bot->setBotCommands();
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "Exception updating commands list: " << e.what();
    }
    if (success) {
        task << InitSuccess;
    } else {
        task << InitFailed;
    }
}

template <>
void init<TgBotWebServer>(InitTask& task) {
    task << "Start webserver";
    constexpr int kTgBotWebServerPort = 8080;
    const auto server =
        ThreadManager::getInstance()
            ->createController<ThreadManager::Usage::WEBSERVER_THREAD,
                               TgBotWebServer>(kTgBotWebServerPort);
    server->onPreStop<TgBotWebServer>([](auto* thread) { thread->stop(); });
    server->run();
    task << InitSuccess;
}

template <>
void init<Random>(InitTask& task) {
    task << "Choose random number generator impl";
    Random::initInstance();
    task << InitSuccess;
}

#ifdef SOCKET_CONNECTION
#include <impl/backends/ServerBackend.hpp>

template <>
void init<SocketInterfaceTgBot>(InitTask& task) {
    task << "Start socket threads";
    auto mgr = ThreadManager::getInstance();
    auto api = TgBotWrapper::getInstance();
    SocketServerWrapper wrapper;
    std::vector<std::shared_ptr<SocketInterfaceTgBot>> threads;
    const auto helper =
        std::make_shared<SocketFile2DataHelper>(std::make_shared<RealFS>());

    if (wrapper.getInternalInterface()) {
        threads.emplace_back(
            mgr->createController<ThreadManager::Usage::SOCKET_THREAD,
                                  SocketInterfaceTgBot>(
                wrapper.getInternalInterface(), api, helper));
    }
    if (wrapper.getExternalInterface()) {
        threads.emplace_back(
            mgr->createController<ThreadManager::Usage::SOCKET_EXTERNAL_THREAD,
                                  SocketInterfaceTgBot>(
                wrapper.getExternalInterface(), api, helper));
    }
    for (auto& thr : threads) {
        thr->run();
    }
    task << InitSuccess;
}

template <>
void init<ChatObserver>(InitTask& task) {
    task << TgBotWrapper::getInitCallNameForClient("ChatObserver");

    TgBotWrapper::getInstance()->onAnyMessage(
        [](const auto, const Message::Ptr& message) {
            const auto& inst = ChatObserver::getInstance();
            if (!inst->observedChatIds.empty() || inst->observeAllChats) {
                inst->process(message);
            }
            return TgBotWrapper::AnyMessageResult::Handled;
        });
    task << InitSuccess;
}

InitTask& operator<<(InitTask& tag, NetworkLogSink& thiz) {
    tag << "Initialize Network LogSink";
    if (!thiz.interface) {
        LOG(ERROR) << "Cannot export log socket, skipping initialization";
        tag << InitFailed;
        return tag;
    }
    thiz.setPreStopFunction([](auto* arg) {
        LOG(INFO) << "onServerShutdown";
        auto* const thiz = static_cast<NetworkLogSink*>(arg);
        thiz->enabled = false;
        thiz->onClientDisconnected.set_value();
    });
    thiz.run();
    tag << InitSuccess;
    return tag;
}

template <>
void init<NetworkLogSink>(InitTask& task) {
    auto handler =
        ThreadManager::getInstance()
            ->createController<ThreadManager::Usage::LOGSERVER_THREAD,
                               NetworkLogSink>();
    task << *handler;
}
#endif

template <>
void init<ResourceManager>(InitTask& task) {
    task << "Load resource manager";
    ResourceManager::getInstance()->preloadResourceDirectory();
    task << InitSuccess;
}

class RegexHandlerInterface : public RegexHandler::Interface {
   public:
    void onError(const absl::Status& status) override {
        _api->sendReplyMessage(
            _message,
            "RegexHandler has encountered an error: " + status.ToString());
    }
    void onSuccess(const std::string& result) override {
        _api->sendReplyMessage(_message->replyToMessage, result);
    }

    explicit RegexHandlerInterface(std::shared_ptr<TgBotApi> api,
                                   Message::Ptr message)
        : _api(std::move(api)), _message(std::move(message)) {}

   private:
    std::shared_ptr<TgBotApi> _api;
    Message::Ptr _message;
};

template <>
void init<RegexHandler>(InitTask& task) {
    task << TgBotWrapper::getInitCallNameForClient("RegexHandler");

    const auto regex = std::make_shared<RegexHandler>();
    TgBotWrapper::getInstance()->onAnyMessage([regex](ApiPtr api,
                                                      MessagePtr message) {
        if (message->has<MessageExt::Attrs::IsReplyMessage,
                         MessageExt::Attrs::ExtraText>() &&
            message->replyToMessage_has<MessageExt::Attrs::ExtraText>()) {
            auto intf = std::make_shared<RegexHandlerInterface>(api, message);
            regex->execute(
                intf,
                message->replyToMessage_get<MessageExt::Attrs::ExtraText>(),
                message->get<MessageExt::Attrs::ExtraText>());
        }
        return TgBotWrapper::AnyMessageResult::Handled;
    });
    task << InitSuccess;
}

template <>
void init<SpamBlockManager>(InitTask& task) {
    task << TgBotWrapper::getInitCallNameForClient("SpamBlockManager");
    TgBotWrapper::getInstance()->onAnyMessage(
        [](const auto, const Message::Ptr& message) {
            static auto spamMgr =
                ThreadManager::getInstance()
                    ->createController<ThreadManager::Usage::SPAMBLOCK_THREAD,
                                       SpamBlockManager>(
                        TgBotWrapper::getInstance());
            spamMgr->addMessage(message);
            return TgBotWrapper::AnyMessageResult::Handled;
        });
    task << InitSuccess;
}

template <>
void init<TgBotDatabaseImpl>(InitTask& task) {
    task << "Loading database";
    auto dbimpl = TgBotDatabaseImpl::getInstance();
    using namespace ConfigManager;
    const auto dbConf = getVariable(Configs::DATABASE_BACKEND);
    std::error_code ec;
    bool loaded = false;

    if (!dbConf) {
        LOG(ERROR) << "No database backend specified in config";
        task << InitFailed;
        return;
    }

    const std::string& config = dbConf.value();
    const auto speratorIdx = config.find(':');

    if (speratorIdx == std::string::npos) {
        LOG(ERROR) << "Invalid database configuration";
        task << InitFailed;
        return;
    }

    // Expected format: <backend>:filename relative to git root (Could be
    // absolute)
    const auto backendStr = config.substr(0, speratorIdx);
    const auto filenameStr = config.substr(speratorIdx + 1);

    TgBotDatabaseImpl::Providers provider;
    if (!provider.chooseProvider(backendStr)) {
        LOG(ERROR) << "Failed to choose provider";
        task << InitFailed;
        return;
    }
    dbimpl->setImpl(std::move(provider));

    if (std::filesystem::path(filenameStr).is_relative()) {
        LOG(WARNING) << "Relative filepaths may result in inconsistent paths";
    }
    loaded = dbimpl->load(filenameStr);
    if (!loaded) {
        LOG(ERROR) << "Failed to load database, the bot will not be able to "
                      "save changes.";
    } else {
        DLOG(INFO) << "Database loaded";
    }
    task << loaded;
}

template <>
void init<ChatDataCollector>(InitTask& task) {
    static ChatDataCollector c;
}

namespace {

void TgBotApiExHandler(const TgBot::TgException& e) {
    static std::optional<DurationPoint> exceptionDuration;
    auto wrapper = TgBotWrapper::getInstance();

    LOG(ERROR) << "Telegram API error: " << "{ Message: "
               << std::quoted(e.what())
               << ", Code: " << static_cast<int32_t>(e.errorCode) << "}";
    switch (e.errorCode) {
        // This is probably bot's runtime problem... Yet it isn't fatal. So
        // skip.
        case TgBot::TgException::ErrorCode::BadRequest:
        // I don't know what to do with this... Skip
        case TgBot::TgException::ErrorCode::Internal:
            return;

        // For floods, this is recoverable, break out of switch to continue
        // recovering.
        case TgBot::TgException::ErrorCode::Flood:
            LOG(INFO) << "Flood detected, trying to recover";
            break;

        // These should be token fault, or network.
        case TgBot::TgException::ErrorCode::Unauthorized:
        case TgBot::TgException::ErrorCode::Forbidden:
        case TgBot::TgException::ErrorCode::NotFound:
            LOG(FATAL) << "FATAL PROBLEM DETECTED";
            break;
        // Only tgbot-cpp library fault can cause this error... Just ignore
        case TgBot::TgException::ErrorCode::InvalidJson:
        case TgBot::TgException::ErrorCode::HtmlResponse:
            LOG(ERROR) << "BUG on tgbot-cpp library";
            return;
        case TgBot::TgException::ErrorCode::Conflict:
            LOG(INFO) << "Conflict detected, shutting down now.";
            OnTerminateRegistrar::getInstance()->callCallbacks();
            std::exit(EXIT_FAILURE);
        default:
            break;
    }

    const auto ownerid = TgBotDatabaseImpl::getInstance()->getOwnerUserId();
    if (ownerid) {
        try {
            wrapper->sendMessage(ownerid.value(),
                                 std::string("Exception occured: ") + e.what());
        } catch (const TgBot::TgException& e) {
            LOG(FATAL) << e.what();
        }
    }
    if (exceptionDuration && exceptionDuration->get() < kErrorMaxDuration) {
        if (ownerid) {
            wrapper->sendMessage(ownerid.value(), "Recovery failed");
        }
        LOG(FATAL) << "Recover failed";
    }
    exceptionDuration.emplace();
    exceptionDuration->init();
    if (ownerid) {
        wrapper->sendMessage(ownerid.value(), "Restarting...");
    }
    LOG(INFO) << "Re-init";
    AuthContext::getInstance()->isAuthorized() = false;
    const auto thrmgr = ThreadManager::getInstance();
    auto cl =
        thrmgr->createController<ThreadManager::Usage::ERROR_RECOVERY_THREAD>();
    if (!cl) {
        cl = thrmgr
                 ->getController<ThreadManager::Usage::ERROR_RECOVERY_THREAD>();
        cl->reset();
    }
    if (cl) {
        cl->runWith([] {
            std::this_thread::sleep_for(kErrorRecoveryDelay);
            AuthContext::getInstance()->isAuthorized() = true;
        });
    }
}

void initLogging() {
    using namespace ConfigManager;
    static std::optional<LogFileSink> log_sink;

    TgBot_AbslLogInit();
    LOG(INFO) << "Registered LogSink_stdout";
    if (const auto it = getVariable(Configs::LOG_FILE); it) {
        log_sink.emplace();
        log_sink->init(*it);
        absl::AddLogSink(&log_sink.value());
        LOG(INFO) << "Register LogSink_file: " << it.value();
    }
}

void createAndDoInitCallAll() {
    InitTask task;

    init<StringResManager>(task);
    init<TgBotWebServer>(task);
    init<Random>(task);

#ifdef SOCKET_CONNECTION
    init<NetworkLogSink>(task);
    init<SocketInterfaceTgBot>(task);
    init<ChatObserver>(task);
#endif
    init<SpamBlockManager>(task);
    init<ResourceManager>(task);
    init<TgBotDatabaseImpl>(task);
    init<RegexHandler>(task);
    init<TgBotWrapper>(task);
    init<ChatDataCollector>(task);

    // Must be last
    OnTerminateRegistrar::getInstance()->registerCallback(
        []() { ThreadManager::getInstance()->destroyManager(); });
}

void onBotInitialized(const std::shared_ptr<TgBotWrapper>& wrapper,
                      DurationPoint& startupDp, const char* exe) {
    LOG(INFO) << "Subsystems initialized, bot started: " << exe;
    LOG(INFO) << "Started in " << startupDp.get().count() << " milliseconds";

    wrapper->setDescriptions(
        "Royna's telegram bot, written in C++. Go on you can talk to him",
        "One of @roynatech's TgBot C++ project bots. I'm currently hosted on "
#if BOOST_OS_WINDOWS
        "Windows"
#elif BOOST_OS_LINUX
        "Linux"
#elif BOOST_OS_MACOS
        "macOS"
#else
        "unknown platform"
#endif
    );
}

}  // namespace

int main(int argc, char** argv) {
    std::optional<std::string> token;
    DurationPoint startupDp;
    using namespace ConfigManager;

    // Insert command line arguments
    CommandLine::initInstance(argc, argv);

    // Initialize logging
    initLogging();

    // Print help and return if help option is set
    if (ConfigManager::getVariable(ConfigManager::Configs::HELP)) {
        ConfigManager::serializeHelpToOStream(std::cout);
        return EXIT_SUCCESS;
    }

    token = getVariable(Configs::TOKEN);
    if (!token) {
        LOG(ERROR) << "Failed to get TOKEN variable";
        return EXIT_FAILURE;
    }

    // Initialize TgBotWrapper instance with provided token
    auto wrapperInst = TgBotWrapper::initInstance(token.value());

    // Install signal handlers
    SignalHandler::install();

    // Initialize subsystems
    createAndDoInitCallAll();

#ifndef WINDOWS_BUILD
    LOG_IF(WARNING, !RestartFmt::handleMessage(wrapperInst).ok())
        << "Failed to handle restart message";
#endif

    try {
        // Bot starts
        onBotInitialized(wrapperInst, startupDp, argv[0]);
    } catch (...) {
    }
    while (!SignalHandler::isSignaled()) {
        try {
            wrapperInst->startPoll();
        } catch (const TgBot::TgException& e) {
            TgBotApiExHandler(e);
        } catch (const std::exception& e) {
            LOG(ERROR) << "Uncaught Exception: " << e.what();
            break;
        }
    }
    OnTerminateRegistrar::getInstance()->callCallbacks();
    return EXIT_SUCCESS;
}
