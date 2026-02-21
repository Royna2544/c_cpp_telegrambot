#include <absl/log/log.h>
#include <absl/log/log_sink_registry.h>
#include <absl/strings/match.h>
#include <absl/strings/str_split.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fruit/fruit.h>
#include <fruit/fruit_forward_decls.h>
#include <fruit/injector.h>

#include <CommandLine.hpp>
#include <ConfigManager.hpp>
#include <DurationPoint.hpp>
#include <LogSinks.hpp>
#include <ManagedThreads.hpp>
#include <ResourceManager.hpp>
#include <TgBotWebpage.hpp>
#include <algorithm>
#include <api/StringResLoader.hpp>
#include <api/TgBotApiImpl.hpp>
#include <cstdlib>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <filesystem>
#include <functional>
#include <global_handlers/RegEXHandler.hpp>
#include <global_handlers/SpamBlockManager.hpp>
#include <libfs.hpp>
#include <libos/libsighandler.hpp>
#include <logging/LoggingServer.hpp>
#include <memory>
#include <ml/ChatDataCollector.hpp>
#include <optional>
#include <restartfmt_parser.hpp>
#include <stdexcept>
#include <thread>
#include <trivial_helpers/fruit_inject.hpp>
#include <utility>

#include "DatabaseBase.hpp"
#include "src/api/net/SocketServiceImpl.hpp"
#include "tgbot/TgException.h"
#include "utils/Env.hpp"

class RegexHandlerInterface : public RegexHandler::Interface {
   public:
    void onError(const tinystatus::TinyStatus& status) override {
        _api->sendReplyMessage(
            _message,
            "RegexHandler has encountered an error: " + status.getMessage());
    }
    void onSuccess(const std::string& result) override {
        _api->sendReplyMessage(_message->replyToMessage, result);
    }

    explicit RegexHandlerInterface(TgBotApi::CPtr api, Message::Ptr message)
        : _api(api), _message(std::move(message)) {}

   private:
    TgBotApi::CPtr _api;
    Message::Ptr _message;
};

template <typename T>
struct Unused {};

template <typename T>
class WrapPtr {
    T* ptr;

   public:
    WrapPtr(T* ptr) : ptr(ptr) {}

    T* pointer() const { return ptr; }
    T* operator->() { return pointer(); }
    operator bool() const { return pointer() != nullptr; }
};

template <typename T>
class WrapSharedPtr {
    std::shared_ptr<T> ptr;

   public:
    WrapSharedPtr(std::shared_ptr<T>&& ptr) : ptr(std::move(ptr)) {}
    T* pointer() const { return ptr.get(); }
    T* operator->() const { return pointer(); }
    operator bool() const { return pointer() != nullptr; }
};

namespace std {
template <>
struct hash<CommandLine> {
    size_t operator()(const CommandLine& ptr) const {
        size_t seed = 0;
        for (int i = 1; i < ptr.argc(); ++i) {
            seed ^= std::hash<std::string_view>{}(ptr.argv()[i]);
        }
        return seed;
    }
};
}  // namespace std

struct SocketChooser {
    std::optional<SocketServiceImpl::Url> primary;
    std::optional<SocketServiceImpl::Url> secondary;
    std::optional<SocketServiceImpl::Url> logging;

    APPLE_EXPLICIT_INJECT(SocketChooser(ConfigManager* manager)) {
        bool firstSet = false;
        if (auto path =
                manager->get(ConfigManager::Configs::SOCKET_URL_PRIMARY);
            path) {
            primary = SocketServiceImpl::Url{*path};
            firstSet = true;
        }
        if (auto path =
                manager->get(ConfigManager::Configs::SOCKET_URL_SECONDARY);
            path) {
            if (!firstSet) {
                LOG(WARNING) << "Secondary socket URL set but primary is not "
                                "set, promoting secondary to primary";
                primary = SocketServiceImpl::Url{*path};
                firstSet = true;
            } else
                secondary = SocketServiceImpl::Url{*path};
        }
        if (auto path =
                manager->get(ConfigManager::Configs::SOCKET_URL_LOGGING);
            path) {
            logging = SocketServiceImpl::Url{*path};
        }
    }
};

namespace {

fruit::Component<fruit::Required<CommandLine>, ConfigManager>
getConfigManagerComponent() {
    return fruit::createComponent().registerProvider(
        [](CommandLine line) { return ConfigManager(std::move(line)); });
}

fruit::Component<fruit::Required<CommandLine>, StringResLoader>
getStringResLoaderComponent() {
    return fruit::createComponent().registerProvider([](CommandLine* cmdline) {
        return StringResLoader(cmdline->getPath(FS::PathType::RESOURCES) /
                               "strings");
    });
}

fruit::Component<fruit::Required<ConfigManager, CommandLine>, TgBotDatabaseImpl,
                 DatabaseBase>
getDatabaseComponent() {
    return fruit::createComponent()
        .bind<DatabaseBase, TgBotDatabaseImpl>()
        .registerProvider([](ConfigManager* manager, CommandLine* cmdline) {
            auto impl = std::make_unique<TgBotDatabaseImpl>();
            if (!TgBotDatabaseImpl_load(manager, impl.get(), cmdline)) {
                LOG(ERROR) << "Failed to load database";
            }
            return impl.release();
        });
}

fruit::Component<fruit::Required<CommandLine>, ResourceProvider>
getResourceProvider() {
    return fruit::createComponent()
        .bind<ResourceProvider, ResourceManager>()
        .registerProvider([](CommandLine* line) -> ResourceManager {
            return ResourceManager(line->getPath(FS::PathType::RESOURCES));
        });
}

fruit::Component<fruit::Required<AuthContext, StringResLoader, Providers,
                                 ConfigManager, RefLock>,
                 TgBotApiImpl, TgBotApi>
getTgBotApiImplComponent() {
    return fruit::createComponent()
        .bind<TgBotApi, TgBotApiImpl>()
        .registerProvider([](AuthContext* auth, StringResLoader* strings,
                             Providers* provider, ConfigManager* config,
                             RefLock* refLock) -> TgBotApiImpl* {
            auto token = config->get(ConfigManager::Configs::TOKEN);
            if (!token) {
                LOG(ERROR) << "Failed to get TOKEN variable";
                throw std::invalid_argument("No TOKEN");
            }

            // Initialize TgBotWrapper instance with provided token
            return new TgBotApiImpl{token.value(), auth, strings, provider,
                                    refLock};
        });
}

#ifdef TGBOTCPP_ENABLE_WEBSERVER
fruit::Component<fruit::Required<ThreadManager, CommandLine>,
                 Unused<TgBotWebServer>> getWebServerComponent() {
    return fruit::createComponent()
        .bind<TgBotWebServerBase, TgBotWebServer>()
        .registerProvider([](ThreadManager* threadManager,
                             CommandLine* cmdline) -> Unused<TgBotWebServer> {
            constexpr int kTgBotWebServerPort = 8080;
            auto* const server = threadManager->create<TgBotWebServer>(
                ThreadManager::Usage::WEBSERVER_THREAD,
                cmdline->getPath(FS::PathType::RESOURCES_WEBPAGE),
                kTgBotWebServerPort);
            server->run();
            return {};
        });
}
#endif

fruit::Component<fruit::Required<ThreadManager, TgBotApi, AuthContext>,
                 WrapPtr<SpamBlockBase>> getSpamBlockComponent() {
    return fruit::createComponent().registerProvider(
        [](ThreadManager* thread, TgBotApi::Ptr api,
           AuthContext* auth) -> WrapPtr<SpamBlockBase> {
            return {thread->create<SpamBlockManager>(
                ThreadManager::Usage::SPAMBLOCK_THREAD, api, auth)};
        });
}

fruit::Component<fruit::Required<ThreadManager, SocketChooser>,
                 Unused<NetworkLogSink>>
getNetworkLogSinkComponent() {
    return fruit::createComponent().registerProvider(
        [](ThreadManager* thread,
           const SocketChooser& config) -> Unused<NetworkLogSink> {
            if (config.logging) {
                thread->create<NetworkLogSink>(
                    ThreadManager::Usage::LOGSERVER_THREAD,
                    config.logging->url);
            }
            return {};
        });
}

using SocketComponentFactory_t = std::function<Unused<SocketServiceImpl>(
    ThreadManager::Usage usage, SocketServiceImpl::Url* path)>;
fruit::Component<fruit::Required<TgBotApi, WrapPtr<SpamBlockBase>,
                                 ThreadManager, DatabaseBase>,
                 SocketComponentFactory_t>
getSocketInterfaceComponent() {
    return fruit::createComponent()
        .registerFactory<Unused<SocketServiceImpl>(
            fruit::Assisted<ThreadManager::Usage> usage,
            fruit::Assisted<SocketServiceImpl::Url*> path, TgBotApi::Ptr api,
            WrapPtr<SpamBlockBase> spamblock, ThreadManager * manager,
            DatabaseBase * database)>(
            [](ThreadManager::Usage usage, SocketServiceImpl::Url* path,
               TgBotApi::Ptr api, WrapPtr<SpamBlockBase> spamblock,
               ThreadManager* manager,
               DatabaseBase* database) -> Unused<SocketServiceImpl> {
                manager
                    ->create<SocketServiceImpl>(usage, api, spamblock.pointer(),
                                                path, database)
                    ->run();
                return {};
            });
}

fruit::Component<fruit::Required<TgBotApi>, Unused<RegexHandler>>
getRegexHandlerComponent() {
    return fruit::createComponent().registerProvider(
        [](TgBotApi::Ptr api) -> Unused<RegexHandler> {
            api->onAnyMessage([](TgBotApi::CPtr api,
                                 const Message::Ptr& message) {
                static auto regex = std::make_unique<RegexHandler>();
                const auto ext = std::make_shared<MessageExt>(message);
                if (ext->has<MessageAttrs::ExtraText>() &&
                    ext->reply()->has<MessageAttrs::ExtraText>()) {
                    auto intf =
                        std::make_shared<RegexHandlerInterface>(api, message);
                    regex->execute(std::move(intf),
                                   ext->reply()->get<MessageAttrs::ExtraText>(),
                                   ext->get<MessageAttrs::ExtraText>());
                }
                return TgBotApi::AnyMessageResult::Handled;
            });
            return {};
        });
}

fruit::Component<TgBotApi, AuthContext, DatabaseBase, ThreadManager,
                 ConfigManager, Unused<RegexHandler>, Unused<NetworkLogSink>,
                 WrapPtr<SpamBlockBase>, RefLock,
#ifdef TGBOTCPP_ENABLE_WEBSERVER
                 Unused<TgBotWebServer>,
#endif
                 SocketComponentFactory_t, SocketChooser, ChatDataCollector>
getAllComponent(CommandLine cmd) {
    static auto _cmd = std::move(cmd);
    return fruit::createComponent()
#ifdef TGBOTCPP_ENABLE_WEBSERVER
        .bind<TgBotWebServerBase, TgBotWebServer>()
#endif
        .bind<RandomBase, Random>()
        .install(getDatabaseComponent)
        .install(getTgBotApiImplComponent)
        .install(getRegexHandlerComponent)
        .install(getNetworkLogSinkComponent)
        .install(getSpamBlockComponent)
#ifdef TGBOTCPP_ENABLE_WEBSERVER
        .install(getWebServerComponent)
#endif
        .install(getStringResLoaderComponent)
        .install(getSocketInterfaceComponent)
        .install(getResourceProvider)
        .install(getConfigManagerComponent)
        .bindInstance(_cmd);
}

std::vector<TgBot::InlineQueryResult::Ptr> mediaQueryKeyboardFunction(
    DatabaseBase* database, std::string_view word) {
    if (word.empty()) {
        return {};  // Nothing to search.
    }
    // Perform search in media database.
    std::vector<TgBot::InlineQueryResult::Ptr> results;
    const auto medias = database->getAllMediaInfos();
    for (const auto& media : medias) {
        for (const auto& name : media.names) {
            if (absl::StartsWith(name, word.data()) || word.empty()) {
                switch (media.mediaType) {
                    case DatabaseBase::MediaType::STICKER: {
                        auto sticker = std::make_shared<
                            TgBot::InlineQueryResultCachedSticker>();
                        sticker->stickerFileId = media.mediaId;
                        sticker->id = media.mediaUniqueId;
                        results.emplace_back(sticker);
                        break;
                    }
                    case DatabaseBase::MediaType::GIF: {
                        auto gif = std::make_shared<
                            TgBot::InlineQueryResultCachedGif>();
                        gif->gifFileId = media.mediaId;
                        gif->id = media.mediaUniqueId;
                        results.emplace_back(gif);
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }
    if (results.empty()) {
        auto noResult = std::make_shared<TgBot::InlineQueryResultArticle>();
        noResult->id = "no_result";
        noResult->title = fmt::format("No results found by '{}'", word);
        noResult->description = "Try searching for something else";
        auto text = std::make_shared<TgBot::InputTextMessageContent>();
        text->messageText = "Try searching for something else";
        noResult->inputMessageContent = text;
        results.emplace_back(noResult);
    }
    return results;
}

struct OptionalComponents {
    bool webServer = false;
    bool dataCollector = false;

    void fromString(const absl::string_view configString) {
        std::vector<std::string> enabledComp =
            absl::StrSplit(configString, ',', absl::SkipWhitespace());
        const auto finder = [&enabledComp](const std::string_view name) {
            return std::ranges::any_of(
                enabledComp,
                [name](const std::string& comp) { return comp == name; });
        };
        webServer = finder("webserver");
        dataCollector = finder("datacollector");
    }
    OptionalComponents() = default;
};
}  // namespace

using std::string_view_literals::operator""sv;

#include <GitBuildInfo.hpp>

#include "command_modules/support/popen_wdt/popen_wdt.h"

void init_work(TgBotApi* api, DatabaseBase* database) {
    TgBotApi::InlineQuery mediaQuery{
        "media", "Get media with the name from database", {}, true, false};

    // Add InlineQuery called 'media'
    api->addInlineQueryKeyboard(
        std::move(mediaQuery),
        [database](
            std::string_view x) -> std::vector<TgBot::InlineQueryResult::Ptr> {
            return mediaQueryKeyboardFunction(database, x);
        });

    // Send a launch message to the owner.
    auto ownerid = database->getOwnerUserId();
    if (ownerid) {
        LOG(INFO) << "Sending a launch message to owner...";
        api->sendMessage(
            *ownerid,
            fmt::format(R"(
Bot @{} has launched.
Platform: {}
Default Shell: {}
Git commit: {}
Git commitmsg: {}
)",
                        *api->getBotUser()->username, buildinfo::OS,
                        POPEN_WDT_DEFAULT_SHELL, buildinfo::git::COMMIT_ID,
                        buildinfo::git::COMMIT_MESSAGE));
    }
}

// Constants for exit codes (similar to sysexits.h)
constexpr int TGBOT_EXITCODE_OK = 0;
constexpr int TGBOT_EXITCODE_GENERIC = 1;
constexpr int TGBOT_EXITCODE_CONFIG = 78;        // EX_CONFIG
constexpr int TGBOT_EXITCODE_NETWORK = 75;       // EX_TEMPFAIL
constexpr int TGBOT_EXITCODE_TELEGRAM_API = 76;  // EX_PROTOCOL
constexpr int TGBOT_EXITCODE_OS = 71;            // EX_OSERR

int app_main(int argc, char** argv) {
    MilliSecondDP startupDp;

    // Delibrately try to access current directory.
    std::error_code ec;
    (void)std::filesystem::current_path(ec);
    if (ec) {
        LOG(ERROR) << "Failed to get current cwd: " << ec.message();
#ifdef _POSIX_C_SOURCE
        struct stat statbuf{};
        if (stat(".", &statbuf) < 0) {
            PLOG(ERROR) << "Couldn't stat cwd";
            return TGBOT_EXITCODE_OS;
        }
        LOG(INFO) << "Current directory: Inode=" << statbuf.st_ino
                  << " NLink=" << statbuf.st_nlink;
        if (statbuf.st_nlink == 0) {
            LOG(INFO) << "This directory is deleted.";
        }
#endif
        return TGBOT_EXITCODE_OS;
    }

    // Install signal handlers
    SignalHandler::install();

    CommandLine cmdline{argc, argv};
    // Initialize dependencies
    fruit::Injector<TgBotApi, AuthContext, DatabaseBase, ThreadManager,
                    ConfigManager, Unused<RegexHandler>, Unused<NetworkLogSink>,
                    WrapPtr<SpamBlockBase>, RefLock,
#ifdef TGBOTCPP_ENABLE_WEBSERVER
                    Unused<TgBotWebServer>,
#endif
                    SocketComponentFactory_t, SocketChooser, ChatDataCollector>
        injector(getAllComponent, cmdline);

    auto configMgr = injector.get<ConfigManager*>();

    auto defLogFile =
        std::filesystem::temp_directory_path(ec) / kDefaultLogFile;

    if (std::filesystem::exists(defLogFile, ec)) {
        LOG(INFO) << "Removing old default log file: " << defLogFile;
        std::filesystem::remove(defLogFile, ec);
    }

    RAIILogSink<LogFileSink> defFileSink(defLogFile);

    // Initialize conditional logging
    RAIILogSink<LogFileSink> logFileSink;

    if (const auto it = configMgr->get(ConfigManager::Configs::LOG_FILE); it) {
        auto sink = std::make_unique<LogFileSink>(*it);
        logFileSink = std::move(sink);
    }

    // Print help and return if help option is set
    if (configMgr->get(ConfigManager::Configs::HELP)) {
        ConfigManager::serializeHelpToOStream(std::cout);
        return TGBOT_EXITCODE_OK;
    }

    // Pre-obtain token, so we won't have to catch exceptions below
    if (!configMgr->get(ConfigManager::Configs::TOKEN)) {
        LOG(ERROR) << "TOKEN is not set, but is required";
        return TGBOT_EXITCODE_CONFIG;
    }

    auto api = injector.get<TgBotApi*>();

    try {
        (void)api->getBotUser();
    } catch (const TgBot::TgException& ex) {
        if (ex.errorCode == TgBot::TgException::ErrorCode::Unauthorized) {
            LOG(ERROR)
                << "API call #getMe returns Unauthorized(401). Exiting...";
            return TGBOT_EXITCODE_TELEGRAM_API;
        }
    } catch (const TgBot::NetworkException& e) {
        LOG(ERROR) << "Network error: " << e.what();
        return TGBOT_EXITCODE_NETWORK;
    }

    auto database = injector.get<DatabaseBase*>();

    // Perform init_work after database but before threadmanager.
    try {
        init_work(api, database);
    } catch (const TgBot::NetworkException& e) {
        LOG(ERROR) << "Network error: " << e.what();
        return TGBOT_EXITCODE_NETWORK;
    } catch (const TgBot::TgException& e) {
        // pass
    }

    auto threadManager = injector.get<ThreadManager*>();

    // Not directly used components
    injector.get<Unused<NetworkLogSink>*>();
    injector.get<Unused<RegexHandler>*>();
    injector.get<WrapPtr<SpamBlockBase>*>();

    OptionalComponents comp{};
    if (auto c = configMgr->get(ConfigManager::Configs::OPTIONAL_COMPONENTS);
        c) {
        comp.fromString(c.value());
    }
#ifdef TGBOTCPP_ENABLE_WEBSERVER
    if (comp.webServer) {
        injector.get<Unused<TgBotWebServer>*>();
    } else {
        DLOG(INFO) << "Skip TgBotWebServer init";
    }
#endif
    if (comp.dataCollector) {
        injector.get<ChatDataCollector*>();
    } else {
        DLOG(INFO) << "Skip ChatDataCollector init";
    }

    auto socketFactor = injector.get<SocketComponentFactory_t>();
    auto _socketServer = injector.get<SocketChooser*>();

    // Initialize actual pointers to injected instances
    if (_socketServer->primary) {
        socketFactor(ThreadManager::Usage::SOCKET_THREAD,
                     &*_socketServer->primary);
    }
    if (_socketServer->secondary) {
        socketFactor(ThreadManager::Usage::SOCKET_EXTERNAL_THREAD,
                     &*_socketServer->secondary);
    }
    LOG_IF(WARNING, !RestartFmt::checkEnvAndVerifyRestart(api))
        << "Failed to handle restart message";

    LOG(INFO) << "Subsystems initialized, bot started: " << argv[0];
    LOG(INFO) << fmt::format("Starting took {}", startupDp.get());

    // Export environment variable for install root
    int exitCode = TGBOT_EXITCODE_OK;

    auto refLock = injector.get<RefLock*>();

    // Retry configuration for network errors
    int retryCount = 0;
    constexpr int kMaxRetries = 10;
    constexpr int kBaseBackoffSeconds = 5;

    while (!SignalHandler::isSignaled()) {
        try {
            api->startPoll();
            retryCount = 0;  // Reset retry count on successful poll
        } catch (const TgBot::NetworkException& e) {
            LOG(ERROR) << "Network error: " << e.what();
            if (++retryCount >= kMaxRetries) {
                LOG(ERROR) << fmt::format(
                    "Max retries ({}) reached for network errors, exiting...",
                    kMaxRetries);
                exitCode = TGBOT_EXITCODE_NETWORK;
                break;
            }
            // Linear backoff (5s, 10s, 15s, ... intervals)
            // This provides a gradual increase suitable for transient network
            // issues
            auto backoffSeconds = kBaseBackoffSeconds * retryCount;
            LOG(WARNING) << fmt::format("Retrying in {}s (attempt {}/{})",
                                        backoffSeconds, retryCount,
                                        kMaxRetries);

            // Sleep with periodic shutdown signal checks for responsive exit
            for (int i = 0; i < backoffSeconds; ++i) {
                if (SignalHandler::isSignaled()) {
                    LOG(INFO)
                        << "Shutdown signal received during retry backoff";
                    goto exit_retry_loop;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } catch (const std::exception& e) {
            LOG(ERROR) << "Uncaught Exception: " << e.what();
            exitCode = TGBOT_EXITCODE_GENERIC;
            break;
        }
    }
exit_retry_loop:
    threadManager->destroy();

    {
        auto lock = refLock->acquireExclusive();
        LOG(INFO) << "All threads, modules have been stopped.";
    }

    LOG(INFO) << fmt::format(
        "{} : exiting now...",
        std::filesystem::path(argv[0]).filename().string());
    return exitCode;
}
