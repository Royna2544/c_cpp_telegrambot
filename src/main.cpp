#include <Authorization.h>
#include <ConfigManager.h>
#include <Database.h>
#include <RegEXHandler.h>
#include <ResourceManager.h>
#include <SingleThreadCtrl.h>
#include <SpamBlock.h>
#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>
#include <command_modules/CommandModule.h>
#include <internal/_std_chrono_templates.h>
#include <libos/libsighandler.h>
#include <socket/SocketInterfaceBase.h>

#include <OnAnyMessageRegister.hpp>

#include "DurationPoint.hpp"

#ifdef RTCOMMAND_LOADER
#include <RTCommandLoader.h>
#endif

#ifdef SOCKET_CONNECTION
#include <ChatObserver.h>

#include <socket/bot/SocketInterfaceInit.hpp>
#endif
#include <tgbot/tgbot.h>

// tgbot
using TgBot::TgLongPoll;

template <typename T>
concept HasInstanceGetter = requires { T::getInstance(); };
template <typename T>
concept HasBotInitCaller = requires(T t, Bot& bot) { t.initWrapper(bot); };
template <typename T>
concept HasBotCtor = requires(Bot& bot) { T(bot); };
template <typename T>
concept DontNeedArguments = std::is_default_constructible_v<T>;

template <typename T>
    requires HasInstanceGetter<T> && HasBotInitCaller<T> && HasBotCtor<T>
void createAndDoInitCall(Bot& bot) {
    T::initInstance(bot);
    T::getInstance().initWrapper(bot);
}

template <typename T>
    requires HasInstanceGetter<T> && HasBotInitCaller<T> && (!HasBotCtor<T>)
void createAndDoInitCall(Bot& bot) {
    T::getInstance().initWrapper(bot);
}

template <typename T>
    requires HasInstanceGetter<T> && (!HasBotInitCaller<T>) && HasBotCtor<T>
void createAndDoInitCall(Bot& bot) {
    T::initInstance(bot);
    T::getInstance().initWrapper();
}

template <typename T>
    requires(!HasInstanceGetter<T>) && HasBotInitCaller<T> && HasBotCtor<T>
void createAndDoInitCall(Bot& bot) {
    T t(bot);
    t.initWrapper(bot);
}

template <typename T>
    requires(!HasInstanceGetter<T>) && HasBotInitCaller<T> &&
            DontNeedArguments<T>
void createAndDoInitCall(Bot& bot) {
    T t;
    t.initWrapper(bot);
}

template <typename T>
    requires HasInstanceGetter<T> &&
             (!HasBotInitCaller<T>) && DontNeedArguments<T>
void createAndDoInitCall(void) {
    T::getInstance().initWrapper();
}

struct LogFileSink : absl::LogSink {
    void Send(const absl::LogEntry& entry) override {
        fputs(entry.text_message_with_prefix_and_newline_c_str(), fp_);
    }
    void init(const std::string& filename) {
        fp_ = fopen(filename.c_str(), "w");
    }
    LogFileSink() = default;
    ~LogFileSink() override {
        if (fp_) {
            fclose(fp_);
        }
    }
    FILE* fp_ = nullptr;
};

int main(int argc, char* const* argv) {
    std::optional<std::string> token;
    std::optional<LogFileSink> log_sink;

    absl::InitializeLog();
    copyCommandLine(CommandLineOp::INSERT, &argc, &argv);
    if (ConfigManager::getVariable("help")) {
        ConfigManager::printHelp();
        return EXIT_SUCCESS;
    }
    if (const auto it = ConfigManager::getVariable("LOG_FILE"); it) {
        log_sink = LogFileSink();
        log_sink->init(*it);
        absl::AddLogSink(&log_sink.value());
        LOG(INFO) << "Register LogSink_file: " << it.value();
    }
    token = ConfigManager::getVariable("TOKEN");
    if (!token) {
        LOG(FATAL) << "Failed to get TOKEN variable";
        return EXIT_FAILURE;
    }

#ifdef HAVE_CURL
    static TgBot::CurlHttpClient cli;
    static Bot gBot(token.value(), cli);
#else
    static Bot gBot(token.value());
#endif

    createAndDoInitCall<RTCommandLoader>(gBot);
    createAndDoInitCall<RegexHandler>(gBot);
    createAndDoInitCall<SpamBlockManager>(gBot);
    createAndDoInitCall<SocketInterfaceInit>(gBot);
    createAndDoInitCall<CommandModule>(gBot);
    createAndDoInitCall<database::DatabaseWrapperBotImplObj>(gBot);
    createAndDoInitCall<ChatObserver>(gBot);
    createAndDoInitCall<ResourceManager>();
    // Must be last
    createAndDoInitCall<OnAnyMessageRegisterer>(gBot);

    installSignalHandler();

    DLOG(INFO) << "Token: " << token.value();
    DurationPoint dp;
    do {
        try {
            LOG(INFO) << "Bot username: " << gBot.getApi().getMe()->username;
            gBot.getApi().deleteWebhook();

            TgLongPoll longPoll(gBot);
            while (true) {
                longPoll.start();
            }
        } catch (const TgBot::TgException& e) {
            LOG(ERROR) << "TgBotAPI Exception: ", e.what();
            LOG(WARNING) << "Trying to recover";
            UserId ownerid = database::DatabaseWrapperImplObj::getInstance()
                                 .maybeGetOwnerId();
            try {
                bot_sendMessage(gBot, ownerid,
                                std::string("Exception occured: ") + e.what());
            } catch (const TgBot::TgException& e) {
                LOG(FATAL) << e.what();
                break;
            }
            if (dp.get() < kErrorMaxDuration) {
                bot_sendMessage(gBot, ownerid, "Recover failed.");
                LOG(FATAL) << "Recover failed";
                break;
            }
            dp.init();
            bot_sendMessage(gBot, ownerid, "Reinitializing.");
            LOG(INFO) << "Re-init";
            gAuthorized = false;
            static const SingleThreadCtrlManager::GetControllerRequest req{
                .usage = SingleThreadCtrlManager::USAGE_ERROR_RECOVERY_THREAD,
                .flags =
                    SingleThreadCtrlManager::REQUIRE_NONEXIST |
                    SingleThreadCtrlManager::REQUIRE_FAILACTION_RETURN_NULL};
            auto cl = SingleThreadCtrlManager::getInstance().getController(req);
            if (cl) {
                cl->runWith([] {
                    std::this_thread::sleep_for(kErrorRecoveryDelay);
                    gAuthorized = true;
                });
            }
        } catch (const std::exception& e) {
            LOG(ERROR) << "Uncaught Exception: ", e.what();
            LOG(ERROR) << "Throwing exception to the main thread";
            defaultCleanupFunction();
            throw;
        }
    } while (true);
    defaultCleanupFunction();
    return EXIT_FAILURE;
}
