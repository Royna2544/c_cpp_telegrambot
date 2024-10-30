#include <ResourceManager.h>
#include <absl/log/log.h>
#include <absl/log/log_sink_registry.h>
#include <absl/strings/match.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fruit/injector.h>

#include <AbslLogInit.hpp>
#include <Authorization.hpp>
#include <ConfigManager.hpp>
#include <DurationPoint.hpp>
#include <LogSinks.hpp>
#include <ManagedThreads.hpp>
#include <Random.hpp>
#include <StringResLoader.hpp>
#include <TgBotWebpage.hpp>
#include <api/TgBotApiImpl.hpp>
#include <boost/system/system_error.hpp>
#include <cstdint>
#include <cstdlib>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <filesystem>
#include <functional>
#include <global_handlers/ChatObserver.hpp>
#include <global_handlers/RegEXHandler.hpp>
#include <global_handlers/SpamBlock.hpp>
#include <impl/backends/ServerBackend.hpp>
#include <impl/bot/TgBotSocketFileHelperNew.hpp>
#include <impl/bot/TgBotSocketInterface.hpp>
#include <libfs.hpp>
#include <libos/libsighandler.hpp>
#include <logging/LoggingServer.hpp>
#include <memory>
#include <ml/ChatDataCollector.hpp>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "DatabaseBase.hpp"
#include "SocketBase.hpp"
#include "api/TgBotApi.hpp"
#include "fruit/fruit.h"
#include "fruit/fruit_forward_decls.h"
#include "trivial_helpers/fruit_inject.hpp"
#ifndef WINDOWS_BUILD
#include <restartfmt_parser.hpp>
#endif

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

    explicit RegexHandlerInterface(TgBotApi::CPtr api, Message::Ptr message)
        : _api(api), _message(std::move(message)) {}

   private:
    TgBotApi::CPtr _api;
    Message::Ptr _message;
};

struct TgBotApiExHandler {
    TgBotApi* api{};
    AuthContext* auth{};
    DatabaseBase* database{};
    ThreadManager* thread{};
    std::optional<DurationPoint> exceptionDuration;

    APPLE_INJECT(TgBotApiExHandler(TgBotApi* _api, AuthContext* _auth,
                                   DatabaseBase* _database,
                                   ThreadManager* _thread))
        : api(_api), auth(_auth), database(_database), thread(_thread) {}

    void handle(const TgBot::TgException& e) {
        LOG(ERROR) << "Telegram API error: " << "{ Message: "
                   << std::quoted(e.what())
                   << ", Code: " << static_cast<int32_t>(e.errorCode) << " }";
        switch (e.errorCode) {
            // This is probably bot's runtime problem... Yet it isn't fatal. So
            // skip.
            case TgBot::TgException::ErrorCode::BadRequest:
                break;
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
                throw e;
            default:
                break;
        }

        const auto ownerid = database->getOwnerUserId();
        if (ownerid) {
            try {
                api->sendMessage(ownerid.value(),
                                 std::string("Exception occured: ") + e.what());
            } catch (const TgBot::TgException& e) {
                LOG(FATAL) << e.what();
            }
        }
        if (exceptionDuration && exceptionDuration->get() < kErrorMaxDuration) {
            if (ownerid) {
                api->sendMessage(ownerid.value(), "Recovery failed");
            }
            LOG(FATAL) << "Recover failed";
        }
        exceptionDuration.emplace();
        exceptionDuration->init();
        if (ownerid) {
            api->sendMessage(ownerid.value(), "Restarting...");
        }
        LOG(INFO) << "Re-init";
        auth->isAuthorized() = false;
        auto* cl =
            thread->create<ThreadManager::Usage::ERROR_RECOVERY_THREAD>();
        if (cl == nullptr) {
            cl = thread->get<ThreadManager::Usage::ERROR_RECOVERY_THREAD>();
            cl->reset();
        }
        if (cl != nullptr) {
            cl->runWith([this] {
                std::this_thread::sleep_for(kErrorRecoveryDelay);
                auth->isAuthorized() = true;
            });
        }
    }
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
struct hash<WrapPtr<ConfigManager>> {
    size_t operator()(const WrapPtr<ConfigManager>& ptr) const {
        // Customize the hashing logic based on your class’s structure
        return std::hash<ConfigManager*>()(
            ptr.pointer());  // Example using pointer hash
    }
};
}  // namespace std

namespace {
fruit::Component<fruit::Required<WrapPtr<ConfigManager>>, ConfigManager>
getConfigManagerComponent() {
    return fruit::createComponent().registerProvider(
        [](WrapPtr<ConfigManager> config) { return config.pointer(); });
}

fruit::Component<fruit::Required<ConfigManager>,
                 WrapSharedPtr<SocketServerWrapper>>
getSocketServerComponent() {
    return fruit::createComponent().registerProvider(
        [](ConfigManager* configManager) -> WrapSharedPtr<SocketServerWrapper> {
            auto value = configManager->get(ConfigManager::Configs::SOCKET_CFG);
            if (!value) {
                LOG(ERROR)
                    << "No socket backend specified, not creating sockets";
                return {nullptr};
            }
            return {std::make_shared<SocketServerWrapper>(value.value())};
        });
}

fruit::Component<StringResLoader, StringResLoaderBase>
getStringResLoaderComponent() {
    return fruit::createComponent()
        .bind<StringResLoaderBase, StringResLoader>()
        .registerProvider([] {
            StringResLoader loader(FS::getPath(FS::PathType::RESOURCES) /
                                   "strings");
            return loader;
        });
}

fruit::Component<fruit::Required<ConfigManager>, TgBotDatabaseImpl,
                 DatabaseBase>
getDatabaseComponent() {
    return fruit::createComponent()
        .bind<DatabaseBase, TgBotDatabaseImpl>()
        .registerProvider([](ConfigManager* manager) {
            auto impl = std::make_unique<TgBotDatabaseImpl>();
            if (!TgBotDatabaseImpl_load(manager, impl.get())) {
                LOG(ERROR) << "Failed to load database";
            }
            return impl.release();
        });
}

fruit::Component<ResourceProvider> getResourceProvider() {
    return fruit::createComponent().bind<ResourceProvider, ResourceManager>();
}

fruit::Component<
    fruit::Required<AuthContext, StringResLoaderBase, Providers, ConfigManager>,
    TgBotApiImpl, TgBotApi>
getTgBotApiImplComponent() {
    return fruit::createComponent()
        .bind<TgBotApi, TgBotApiImpl>()
        .registerProvider([](AuthContext* auth, StringResLoaderBase* strings,
                             Providers* provider,
                             ConfigManager* config) -> TgBotApiImpl* {
            auto token = config->get(ConfigManager::Configs::TOKEN);
            if (!token) {
                LOG(ERROR) << "Failed to get TOKEN variable";
                throw std::invalid_argument("No TOKEN");
            }

            // Initialize TgBotWrapper instance with provided token
            auto bot = new TgBotApiImpl(token.value(), auth, strings, provider);

            // Load modules
            std::filesystem::path modules_path = config->exe().parent_path();
            std::error_code ec;
            
            LOG(INFO) << "Loading commands from " << modules_path;
            for (const auto& it :
                 std::filesystem::directory_iterator(modules_path, ec)) {
                if (it.path().filename().string().starts_with(
                        CommandModule::prefix)) {
                    bot->addCommand(std::make_unique<CommandModule>(it));
                }
            }
            if (ec) {
                LOG(ERROR) << "Failed to iterate through modules: "
                           << ec.message();
                return bot;
            }
            try {
                LOG(INFO) << "Updating botcommands with loaded modules...";
                bot->setBotCommands();
            } catch (const boost::system::system_error& e) {
                LOG(ERROR) << "Exception updating commands list: " << e.what();
            }
            return bot;
        });
}

fruit::Component<Unused<TgBotWebServer>> getWebServerComponent() {
    return fruit::createComponent()
        .bind<TgBotWebServerBase, TgBotWebServer>()
        .registerProvider([](ThreadManager* threadManager)
                              -> Unused<TgBotWebServer> {
            constexpr int kTgBotWebServerPort = 8080;
            const auto server =
                threadManager->create<ThreadManager::Usage::WEBSERVER_THREAD,
                                      TgBotWebServer>(kTgBotWebServerPort);
            server->run();
            return {};
        });
}

fruit::Component<fruit::Required<ThreadManager, TgBotApi, AuthContext>,
                 WrapPtr<SpamBlockBase>>
getSpamBlockComponent() {
    return fruit::createComponent().registerProvider(
        [](ThreadManager* thread, TgBotApi::Ptr api,
           AuthContext* auth) -> WrapPtr<SpamBlockBase> {
            return {thread->create<ThreadManager::Usage::SPAMBLOCK_THREAD,
                                   SpamBlockManager>(api, auth)};
        });
}

fruit::Component<
    fruit::Required<ThreadManager, WrapSharedPtr<SocketServerWrapper>>,
    Unused<NetworkLogSink>>
getNetworkLogSinkComponent() {
    return fruit::createComponent().registerProvider(
        [](ThreadManager* thread,
           const WrapSharedPtr<SocketServerWrapper>& config)
            -> Unused<NetworkLogSink> {
            thread->create<ThreadManager::Usage::LOGSERVER_THREAD,
                           NetworkLogSink>(config.pointer());
            return {};
        });
}

using SocketComponentFactory_t = std::function<Unused<SocketInterfaceTgBot>(
    ThreadManager::Usage usage, SocketInterfaceBase*)>;
fruit::Component<
    fruit::Required<TgBotApi, ChatObserver, WrapPtr<SpamBlockBase>,
                    SocketFile2DataHelper, ThreadManager, ResourceProvider>,
    SocketComponentFactory_t>
getSocketInterfaceComponent() {
    return fruit::createComponent()
        .registerFactory<Unused<SocketInterfaceTgBot>(
            fruit::Assisted<ThreadManager::Usage> usage,
            fruit::Assisted<SocketInterfaceBase*> interface, TgBotApi::Ptr api,
            ChatObserver * observer, WrapPtr<SpamBlockBase> spamblock,
            SocketFile2DataHelper * helper, ThreadManager * manager,
            ResourceProvider * resource)>(
            [](ThreadManager::Usage usage, SocketInterfaceBase* interface,
               TgBotApi::Ptr api, ChatObserver* observer,
               WrapPtr<SpamBlockBase> spamblock, SocketFile2DataHelper* helper,
               ThreadManager* manager,
               ResourceProvider* resource) -> Unused<SocketInterfaceTgBot> {
                auto thread = manager->create<SocketInterfaceTgBot>(
                    usage, interface, api, observer, spamblock.pointer(),
                    helper, resource);
                thread->run();
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
                    ext->replyMessage()->has<MessageAttrs::ExtraText>()) {
                    auto intf =
                        std::make_shared<RegexHandlerInterface>(api, message);
                    regex->execute(
                        std::move(intf),
                        ext->replyMessage()->get<MessageAttrs::ExtraText>(),
                        ext->get<MessageAttrs::ExtraText>());
                }
                return TgBotApi::AnyMessageResult::Handled;
            });
            return {};
        });
}

fruit::Component<TgBotApi, AuthContext, DatabaseBase, ThreadManager,
                 Unused<RegexHandler>, Unused<NetworkLogSink>,
                 WrapPtr<SpamBlockBase>, Unused<TgBotWebServer>,
                 TgBotApiExHandler, SocketComponentFactory_t,
                 WrapSharedPtr<SocketServerWrapper>>
getAllComponent(WrapPtr<ConfigManager> config) {
    static const auto _config = config;
    return fruit::createComponent()
        .bind<TgBotWebServerBase, TgBotWebServer>()
        .bind<VFSOperations, RealFS>()
        .install(getDatabaseComponent)
        .install(getTgBotApiImplComponent)
        .install(getRegexHandlerComponent)
        .install(getNetworkLogSinkComponent)
        .install(getSpamBlockComponent)
        .install(getWebServerComponent)
        .install(getStringResLoaderComponent)
        .install(getSocketInterfaceComponent)
        .install(getResourceProvider)
        .install(getConfigManagerComponent)
        .install(getSocketServerComponent)
        .bindInstance(_config);
}

std::vector<TgBot::InlineQueryResult::Ptr> mediaQueryKeyboardFunction(
    DatabaseBase* database, std::string_view word) {
    std::vector<TgBot::InlineQueryResult::Ptr> results;
    const auto medias = database->getAllMediaInfos();
    for (const auto& media : medias) {
        for (const auto& name : media.names) {
            if (absl::StartsWith(name, word)) {
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
        noResult->title = fmt::format("No results found by {}", word);
        noResult->description = "Try searching for something else";
        auto text = std::make_shared<TgBot::InputTextMessageContent>();
        text->messageText = "Try searching for something else";
        noResult->inputMessageContent = text;
        results.emplace_back(noResult);
    }
    return results;
}

}  // namespace

using std::string_view_literals::operator""sv;

int main(int argc, char** argv) {
    DurationPoint startupDp;

    // Initialize Abseil logging system
    TgBot_AbslLogInit();

    // Create configmanager
    auto configMgr = std::make_unique<ConfigManager>(argc, argv);

    // Initialize logging
    RAIILogSink<LogFileSink> logFileSink;

    LOG(INFO) << "Registered LogSink_stdout";
    if (const auto it = configMgr->get(ConfigManager::Configs::LOG_FILE); it) {
        auto sink = std::make_unique<LogFileSink>(*it);
        LOG(INFO) << "Register LogSink_file: " << it.value();
        logFileSink = std::move(sink);
    }

    // Print help and return if help option is set
    if (configMgr->get(ConfigManager::Configs::HELP)) {
        ConfigManager::serializeHelpToOStream(std::cout);
        return EXIT_SUCCESS;
    }

    // Pre-obtain token, so we won't have to catch exceptions below
    if (!configMgr->get(ConfigManager::Configs::TOKEN)) {
        LOG(ERROR) << "TOKEN is not set, but is required";
        return EXIT_FAILURE;
    }

    // Install signal handlers
    SignalHandler::install();

    // Transfer ConfigManager ownership
    WrapPtr configPtr{configMgr.release()};

    // Initialize dependencies
    fruit::Injector<TgBotApi, AuthContext, DatabaseBase, ThreadManager,
                    Unused<RegexHandler>, Unused<NetworkLogSink>,
                    WrapPtr<SpamBlockBase>, Unused<TgBotWebServer>,
                    TgBotApiExHandler, SocketComponentFactory_t,
                    WrapSharedPtr<SocketServerWrapper>>
        injector(getAllComponent, configPtr);

    auto threadManager = injector.get<ThreadManager*>();
    auto api = injector.get<TgBotApi*>();
    auto database = injector.get<DatabaseBase*>();
    auto exHandle = injector.get<TgBotApiExHandler*>();

    // Not directly used components
    injector.get<Unused<NetworkLogSink>*>();
    injector.get<Unused<RegexHandler>*>();
    injector.get<WrapPtr<SpamBlockBase>*>();
    injector.get<Unused<TgBotWebServer>*>();

    auto socketFactor = injector.get<SocketComponentFactory_t>();
    auto _socketServer = injector.get<WrapSharedPtr<SocketServerWrapper>>();
    std::shared_ptr<SocketInterfaceBase> socketInternal;
    std::shared_ptr<SocketInterfaceBase> socketExternal;

    // Initialize actual pointers to injected instances
    if (_socketServer.pointer() != nullptr) {
        if (socketInternal = _socketServer->getInternalInterface();
            socketInternal) {
            socketFactor(ThreadManager::Usage::SOCKET_THREAD,
                         socketInternal.get());
        }
        if (socketExternal = _socketServer->getExternalInterface();
            socketExternal) {
            socketFactor(ThreadManager::Usage::SOCKET_EXTERNAL_THREAD,
                         socketExternal.get());
        }
    }

#ifndef WINDOWS_BUILD
    LOG_IF(WARNING, !RestartFmt::handleMessage(api).ok())
        << "Failed to handle restart message";
#endif

    LOG(INFO) << "Subsystems initialized, bot started: " << argv[0];
    LOG(INFO) << fmt::format("Starting took {}", startupDp.get());

    api->setDescriptions(
        "Royna's telegram bot, written in C++. Go on you can talk to it"sv,
        "One of @roynatech's TgBot C++ project bots. I'm currently hosted "
        "on "
#ifdef WINDOWS_BUILD
        "Windows"sv
#elif defined(__linux__)
        "Linux"sv
#elif defined(__APPLE__)
        "macOS"sv
#else
        "unknown platform"sv
#endif
    );

    api->addInlineQueryKeyboard(
        TgBotApi::InlineQuery{"media", "Get media with the name from database",
                              true, false},
        [database](
            std::string_view x) -> std::vector<TgBot::InlineQueryResult::Ptr> {
            return mediaQueryKeyboardFunction(database, x);
        });
    while (!SignalHandler::isSignaled()) {
        try {
            api->startPoll();
        } catch (const TgBot::TgException& e) {
            exHandle->handle(e);
        } catch (const std::exception& e) {
            LOG(ERROR) << "Uncaught Exception: " << e.what();
            break;
        }
    }
    threadManager->destroyManager();
    return EXIT_SUCCESS;
}
