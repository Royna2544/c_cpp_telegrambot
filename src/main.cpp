#include <Authorization.h>
#include <ConfigManager.h>
#include <ResourceManager.h>
#include <absl/log/log.h>
#include <absl/log/log_sink_registry.h>
#include <internal/_std_chrono_templates.h>

#include <AbslLogInit.hpp>
#include <CommandLine.hpp>
#include <DurationPoint.hpp>
#include <LogSinks.hpp>
#include <ManagedThreads.hpp>
#include <RegEXHandler.hpp>
#include <SpamBlock.hpp>
#include <StringResManager.hpp>
#include <TgBotWebpage.hpp>
#include <TryParseStr.hpp>
#include <boost/system/system_error.hpp>
#include <chrono>
#include <cstddef>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <filesystem>
#include <libos/libfs.hpp>
#include <libos/libsighandler.hpp>
#include <memory>
#include <ml/ChatDataCollector.hpp>
#include <utility>

#include "Random.hpp"
#include "TgBotWrapper.hpp"
#include "tgbot/TgException.h"

#ifndef WINDOWS_BUILD
#include <restartfmt_parser.hpp>
#endif

#ifdef SOCKET_CONNECTION
#include <ChatObserver.h>

#include <socket/interface/impl/bot/TgBotSocketInterface.hpp>

#include "logging/LoggingServer.hpp"
#endif

#include <tgbot/tgbot.h>

// tgbot
using TgBot::TgLongPoll;

template <typename T>
concept HasInstanceGetter = requires { T::getInstance(); };
template <typename T>
concept HasBotInitCaller = requires(T t, Bot& bot) { t.initWrapper(bot); };
template <typename T>
concept HasInitCaller = requires(T t) { t.initWrapper(); };

template <typename T, typename... Args>
    requires HasInstanceGetter<T> && HasBotInitCaller<T>
void createAndDoInitCall(Bot& bot, Args&&... args) {
    if constexpr (sizeof...(args) != 0) {
        T::initInstance(std::forward<Args>(args)...);
    }
    T::getInstance()->initWrapper(bot);
}

template <typename T, typename... Args>
    requires HasInstanceGetter<T> && HasInitCaller<T>
void createAndDoInitCall(Args&&... args) {
    if constexpr (sizeof...(args) != 0) {
        T::initInstance(std::forward<Args>(args)...);
    }
    T::getInstance()->initWrapper();
}

template <typename T, typename... Args>
    requires(!HasInstanceGetter<T>) && HasBotInitCaller<T>
void createAndDoInitCall(Bot& bot, Args&&... args) {
    if constexpr (sizeof...(args) != 0) {
        T t(std::forward<Args>(args)...);
        t.initWrapper(bot);
    } else {
        T t;
        t.initWrapper(bot);
    }
}

template <typename T, typename... Args>
    requires(!HasInstanceGetter<T>) && HasInitCaller<T>
void createAndDoInitCall(Args&&... args) {
    if constexpr (sizeof...(args) != 0) {
        T t(std::forward<Args>(args)...);
        t.initWrapper();
    } else {
        T t;
        t.initWrapper();
    }
}

template <typename T, ThreadManager::Usage usage, typename... Args>
    requires(HasInitCaller<T>)
void createAndDoInitCall(Args... args) {
    const auto mgr = ThreadManager::getInstance();
    std::shared_ptr<T> inst;
    if constexpr (sizeof...(args) != 0) {
        inst = mgr->createController<usage, T>(std::forward<Args...>(args...));
    } else {
        inst = mgr->createController<usage, T>();
    }
    inst->initWrapper();
}

namespace {

void TgBotApiExHandler(const TgBot::TgException& e) {
    static std::optional<DurationPoint> exceptionDuration;
    auto wrapper = TgBotWrapper::getInstance();

    LOG(ERROR) << "Telegram API error: " << "{ Message: "
               << std::quoted(e.what())
               << ", Code: " << static_cast<size_t>(e.errorCode) << "}";
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

void createAndDoInitCallAll() {
    constexpr int kWebServerListenPort = 8080;

    const auto bot = TgBotWrapper::getInstance();
    createAndDoInitCall<StringResManager>();
    createAndDoInitCall<TgBotWebServer, ThreadManager::Usage::WEBSERVER_THREAD>(
        kWebServerListenPort);
    createAndDoInitCall<Random>(nullptr);

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

    LOG(INFO) << "Updating commands list based on loaded commands...";
    try {
        LOG_IF(ERROR, !bot->setBotCommands())
            << "Couldn't update commands list";
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "Error updating commands list: " << e.what();
    }
    LOG(INFO) << "...done";

#ifdef SOCKET_CONNECTION
    createAndDoInitCall<NetworkLogSink,
                        ThreadManager::Usage::LOGSERVER_THREAD>();
    createAndDoInitCall<SocketInterfaceTgBot>(
        nullptr, bot,
        std::make_shared<SocketFile2DataHelper>(std::make_shared<RealFS>()));
    createAndDoInitCall<ChatObserver>();
#endif
    createAndDoInitCall<SpamBlockManager>(bot);
    createAndDoInitCall<ResourceManager>();
    createAndDoInitCall<TgBotDatabaseImpl>();

    const auto regex = std::make_shared<RegexHandler>();
    bot->onAnyMessage([regex](ApiPtr api, MessagePtr message) {
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

    ChatDataCollector collector;
    collector.initWrapper();

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
