#include <Authorization.h>
#include <BotAddCommand.h>
#include <ConfigManager.h>
#include <Database.h>
#include <RegEXHandler.h>
#include <ResourceManager.h>
#include <SingleThreadCtrl.h>
#include <SpamBlock.h>
#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <absl/log/internal/flags.h>
#include <absl/flags/flag.h>
#include <command_modules/CommandModule.h>
#include <internal/_std_chrono_templates.h>
#include <libos/libfs.h>
#include <libos/libsighandler.h>
#include <socket/SocketInterfaceBase.h>

#include <OnAnyMessageRegister.hpp>
#include <initcalls/BotInitcall.hpp>
#include <type_traits>

#ifdef RTCOMMAND_LOADER
#include <RTCommandLoader.h>
#endif

#ifdef SOCKET_CONNECTION
#include <ChatObserver.h>

#include <socket/bot/SocketInterfaceInit.hpp>
#endif
#include <tgbot/tgbot.h>

// wingdi.h
#undef ERROR

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

int main(int argc, char* const* argv) {
    std::string token;

    absl::SetFlag(&FLAGS_stderrthreshold, 0);
    absl::InitializeLog();
    copyCommandLine(CommandLineOp::INSERT, &argc, &argv);
    if (ConfigManager::getVariable("help")) {
        ConfigManager::printHelp();
        return EXIT_SUCCESS;
    }
    auto ret = ConfigManager::getVariable("TOKEN");
    if (!ret) {
        LOG(FATAL) << "Failed to get TOKEN variable";
        return EXIT_FAILURE;
    }
    token = ret.value();

    static Bot gBot(token);
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

    DLOG(INFO) << "Token: " << token;
    auto CurrentTp = std::chrono::system_clock::now();
    auto LastTp = std::chrono::system_clock::from_time_t(0);
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
            CurrentTp = std::chrono::system_clock::now();
            if (to_secs(CurrentTp - LastTp).count() < 30 &&
                std::chrono::system_clock::to_time_t(LastTp) != 0) {
                bot_sendMessage(gBot, ownerid, "Recover failed.");
                LOG(FATAL) << "Recover failed";
                break;
            }
            LastTp = CurrentTp;
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
            throw;
        }
    } while (true);
    defaultCleanupFunction();
    return EXIT_FAILURE;
}
