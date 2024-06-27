#include <Authorization.h>
#include <ConfigManager.h>
#include <RegEXHandler.h>
#include <ResourceManager.h>
#include <SpamBlock.h>
#include <absl/log/log.h>
#include <absl/log/log_sink_registry.h>
#include <command_modules/CommandModule.h>
#include <internal/_std_chrono_templates.h>
#include <libos/libsighandler.h>

#include <AbslLogInit.hpp>
#include <DurationPoint.hpp>
#include <LogSinks.hpp>
#include <ManagedThreads.hpp>
#include <OnAnyMessageRegister.hpp>
#include <StringResManager.hpp>
#include <TgBotWebpage.hpp>
#include <TryParseStr.hpp>
#include <boost/algorithm/string/split.hpp>
#include <chrono>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <memory>
#include <utility>

#include "tgbot/Bot.h"

#ifdef RTCOMMAND_LOADER
#include <RTCommandLoader.h>
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
void handleRestartCommand(Bot& bot) {
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
            bot.getApi().sendMessage(
                chatId, "Restart success!", nullptr,
                std::make_shared<TgBot::ReplyParameters>(msgId, chatId, true));
        } else {
            LOG(ERROR) << "Could not parse back params!";
        }
    }
}

void TgBotApiExHandler(TgBot::Bot& bot, const TgBot::TgException& e) {
    static std::optional<DurationPoint> exceptionDuration;

    LOG(ERROR) << "TgBotAPI Exception: " << e.what();
    LOG(WARNING) << "Trying to recover";
    UserId ownerid = TgBotDatabaseImpl::getInstance()->getOwnerUserId();
    try {
        bot_sendMessage(bot, ownerid,
                        std::string("Exception occured: ") + e.what());
    } catch (const TgBot::TgException& e) {
        LOG(FATAL) << e.what();
    }
    if (exceptionDuration && exceptionDuration->get() < kErrorMaxDuration) {
        bot_sendMessage(bot, ownerid, "Recover failed.");
        LOG(FATAL) << "Recover failed";
    }
    exceptionDuration.emplace();
    exceptionDuration->init();
    bot_sendMessage(bot, ownerid, "Reinitializing.");
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

void createAndDoInitCallAll(TgBot::Bot& gBot) {
    constexpr int kWebServerListenPort = 8080;
    createAndDoInitCall<StringResManager>();
    createAndDoInitCall<TgBotWebServer, ThreadManager::Usage::WEBSERVER_THREAD>(
        kWebServerListenPort);
#ifdef RTCOMMAND_LOADER
    createAndDoInitCall<RTCommandLoader>(gBot);
#endif
#ifdef SOCKET_CONNECTION
    createAndDoInitCall<NetworkLogSink,
                        ThreadManager::Usage::LOGSERVER_THREAD>();
    createAndDoInitCall<SocketInterfaceTgBot>(gBot, gBot, nullptr);
    createAndDoInitCall<ChatObserver>();
#endif
    createAndDoInitCall<RegexHandler>(gBot);
    createAndDoInitCall<SpamBlockManager>(gBot);
    createAndDoInitCall<CommandModuleManager>(gBot);
    createAndDoInitCall<ResourceManager>();
    createAndDoInitCall<TgBotDatabaseImpl>();
    // Must be last
    createAndDoInitCall<OnAnyMessageRegisterer>(gBot);
}

void onBotInitialized(TgBot::Bot& gBot, DurationPoint& startupDp,
                      const char* exe) {
    LOG(INFO) << "Subsystems initialized, bot started: " << exe;
    LOG(INFO) << "Started in " << startupDp.get().count() << " milliseconds";

    gBot.getApi().setMyDescription(
        "Royna's telegram bot, written in C++. Go on you can talk to him");
    gBot.getApi().setMyShortDescription(
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

int main(int argc, char* const* argv) {
    std::optional<std::string> token;
    DurationPoint startupDp;
    using namespace ConfigManager;

    // Initialize logging
    initLogging();

    // Insert command line arguments
    copyCommandLine(CommandLineOp::INSERT, &argc, &argv);

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

#ifdef HAVE_CURL
    TgBot::CurlHttpClient cli;
    Bot gBot(token.value(), cli);
#else
    Bot gBot(token.value());
#endif

    // Install signal handlers
    installSignalHandler();

    // Initialize subsystems
    createAndDoInitCallAll(gBot);

#ifndef WINDOWS_BUILD
    handleRestartCommand(gBot);
#endif

    try {
        // Bot starts
        onBotInitialized(gBot, startupDp, argv[0]);
    } catch (...) {
    }
    while (true) {
        try {
            LOG(INFO) << "Bot username: " << gBot.getApi().getMe()->username;
            gBot.getApi().deleteWebhook();

            TgLongPoll longPoll(gBot);
            while (true) {
                longPoll.start();
            }
        } catch (const TgBot::TgException& e) {
            TgBotApiExHandler(gBot, e);
        } catch (const std::exception& e) {
            LOG(ERROR) << "Uncaught Exception: " << e.what();
            LOG(ERROR) << "Throwing exception to the main thread";
            defaultCleanupFunction();
            throw;
        }
    }
    defaultCleanupFunction();
    return EXIT_FAILURE;
}
