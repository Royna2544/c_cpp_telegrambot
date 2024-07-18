#include <Authorization.h>
#include <ConfigManager.h>
#include <RTCommandLoader.h>
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
#include <boost/algorithm/string/split.hpp>
#include <chrono>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <libos/libsighandler.hpp>
#include <memory>
#include <utility>

#include "InstanceClassBase.hpp"
#include "TgBotWrapper.hpp"
#include "tgbot/Bot.h"

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
        T t(std::forward<Args...>(args...));
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
void handleRestartCommand(const std::shared_ptr<TgBotWrapper>& wrapper) {
    // If it was restarted, then send a message to the caller
    std::string result;
    std::vector<std::string> splitStrings(2);
    if (ConfigManager::getEnv("RESTART", result)) {
        MessageId msgId = 0;
        ChatId chatId = 0;
        LOG(INFO) << "RESTART env var set to " << result;

        boost::split(splitStrings, result, [](char c) { return c == ':'; });

        if (splitStrings.size() != 2) {
            LOG(ERROR) << "Invalid RESTART env var format";
        } else if (try_parse(splitStrings[0], &chatId) &&
                   try_parse(splitStrings[1], &msgId)) {
            LOG(INFO) << "Restart success!";
            wrapper->sendReplyMessage(chatId, msgId, "Restart success!");
        } else {
            LOG(ERROR) << "Could not parse back params!";
        }
    }
}

void TgBotApiExHandler(const TgBot::TgException& e) {
    static std::optional<DurationPoint> exceptionDuration;
    auto wrapper = TgBotWrapper::getInstance();

    LOG(ERROR) << "TgBotAPI Exception: " << e.what();
    LOG(WARNING) << "Trying to recover";
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
    constexpr int kWebServerListenPort = 8080;

    const auto bot = TgBotWrapper::getInstance();
    createAndDoInitCall<StringResManager>();
    createAndDoInitCall<TgBotWebServer, ThreadManager::Usage::WEBSERVER_THREAD>(
        kWebServerListenPort);
    createAndDoInitCall<RTCommandLoader>();
    LOG(INFO) << "Updating commands list based on loaded commands...";
    LOG_IF(ERROR, !bot->setBotCommands()) << "Couldn't update commands list";
    LOG(INFO) << "...done";

#ifdef SOCKET_CONNECTION
    createAndDoInitCall<NetworkLogSink,
                        ThreadManager::Usage::LOGSERVER_THREAD>();
    createAndDoInitCall<SocketInterfaceTgBot>(nullptr);
    createAndDoInitCall<ChatObserver>();
#endif
    createAndDoInitCall<RegexHandler>();
    createAndDoInitCall<SpamBlockManager>(bot);
    createAndDoInitCall<ResourceManager>();
    createAndDoInitCall<TgBotDatabaseImpl>();
    bot->registerOnAnyMsgCallback();
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
    handleRestartCommand(wrapperInst);
#endif

    try {
        // Bot starts
        onBotInitialized(wrapperInst, startupDp, argv[0]);
    } catch (...) {
    }
    while (!SignalHandler::isSignaled()) {
        try {
            LOG(INFO) << "Bot username: "
                      << wrapperInst->getBotUser()->username;
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
