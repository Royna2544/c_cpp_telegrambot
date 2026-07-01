#include <absl/strings/str_cat.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>
#include <google/protobuf/empty.pb.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <tgbot/TgException.h>
#include <tgbot/types/InlineQueryResultArticle.h>
#include <tgbot/types/InputFile.h>
#include <tgbot/types/InputTextMessageContent.h>
#include <trivial_helpers/_tgbot.h>

#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <unordered_map>
#include <utility>

#include "BuildObserver.hpp"
#include "ConfigManager.hpp"
#include "ConfigParsers2.hpp"
#include "GrpcLinuxKernelBuildService.hpp"
#include "HealthCheck_service.grpc.pb.h"
#include "ILinuxKernelBuildService.hpp"
#include "KernelBuildOrchestrator.hpp"
#include "LinuxKernelBuild_service.grpc.pb.h"
#include "LinuxKernelBuild_service.pb.h"
#include "SystemMonitor_service.grpc.pb.h"
#include "SystemMonitor_service.pb.h"
#include "SystemStatsProvider.hpp"
#include "api/AuthContext.hpp"
#include "api/RateLimit.hpp"
#include "api/TgBotApi.hpp"
#include "command_modules/support/KeyBoardBuilder.hpp"
#include "tgbot/types/InlineKeyboardMarkup.h"

using tgbot::builder::linuxkernel::LinuxKernelBuildService;
using tgbot::builder::system_monitor::SystemMonitorService;

namespace {

// Edit the message backing a callback query, if it is still accessible.
template <TgBotApi::ParseMode mode = TgBotApi::ParseMode::None>
void editCallbackMessage(
    const TgBotApi::Ptr& api, const TgBot::CallbackQuery::Ptr& query,
    const std::string& text,
    const TgBot::InlineKeyboardMarkup::Ptr& replyMarkup = nullptr) {
    if (query->message) {
        if (auto msg = std::get_if<TgBot::Message::Ptr>(&(*query->message))) {
            api->editMessage<mode>(*msg, text, replyMarkup);
            return;
        }
    }
    LOG(ERROR) << "Query message inaccessible, cannot edit message";
}

/**
 * @brief Telegram presenter for kernel build progress.
 *
 * Implements the transport-agnostic IBuildObserver: it throttles message edits,
 * pulls a system snapshot only when it actually renders, and never leaks into
 * the orchestration logic.
 */
class TgKernelObserver : public tgbot::builder::IBuildObserver {
   public:
    TgKernelObserver(TgBotApi::Ptr api, TgBot::CallbackQuery::Ptr query,
                     const tgbot::builder::SystemStatsProvider* stats,
                     std::chrono::system_clock::time_point startTime,
                     std::string kernelName, std::string device,
                     TgBot::InlineKeyboardMarkup::Ptr keyboard)
        : api_(std::move(api)),
          query_(std::move(query)),
          stats_(stats),
          start_(startTime),
          kernelName_(std::move(kernelName)),
          device_(std::move(device)),
          keyboard_(std::move(keyboard)),
          limiter_(1, std::chrono::seconds(5)) {}

    void onProgress(const tgbot::builder::ProgressEvent& event) override {
        if (!limiter_.check()) {
            return;
        }
        tgbot::builder::SystemSnapshot snap;
        if (stats_ != nullptr) {
            snap = stats_->snapshot();
        }
        auto msg = fmt::format(
            R"(
<blockquote>Start Time: {} (GMT)
Time Spent: {:%M minutes %S seconds}
Kernel Name: {}</blockquote>
<blockquote>📱 <b>Device</b>: {}
💻 <b>CPU</b>: {:.2f}%
💾 <b>Memory</b>: {}MB / {}MB</blockquote>
<blockquote>{}</blockquote>)",
            start_, std::chrono::system_clock::now() - start_, kernelName_,
            device_, snap.cpuUsagePercent, snap.memUsedMb, snap.memTotalMb,
            event.message);
        try {
            editCallbackMessage<TgBotApi::ParseMode::HTML>(api_, query_, msg,
                                                           keyboard_);
        } catch (const TgBot::TgException& e) {
            LOG(ERROR) << "Failed to edit message: " << e.what();
        }
    }

    void onFailed(std::string_view message) override {
        LOG(ERROR) << message;
        editCallbackMessage(api_, query_, std::string(message));
    }

    void onCompleted(std::string_view message) override {
        editCallbackMessage(api_, query_, std::string(message));
    }

   private:
    TgBotApi::Ptr api_;
    TgBot::CallbackQuery::Ptr query_;
    const tgbot::builder::SystemStatsProvider* stats_;
    std::chrono::system_clock::time_point start_;
    std::string kernelName_;
    std::string device_;
    TgBot::InlineKeyboardMarkup::Ptr keyboard_;
    IntervalRateLimiter limiter_;
};

}  // namespace

class KernelBuildHandler {
   public:
    struct Intermidates {
        KernelConfig* current{};
        std::string device;
        std::unordered_map<std::string, bool> fragment_preference;
        std::chrono::system_clock::time_point start_time;
        std::atomic_int build_id{0};

        Intermidates() = default;
        Intermidates& operator=(Intermidates&& other) noexcept {
            if (this != &other) {
                current = other.current;
                device = std::move(other.device);
                fragment_preference = std::move(other.fragment_preference);
                start_time = other.start_time;
                build_id.store(other.build_id.load());
            }
            return *this;
        }
        Intermidates(const Intermidates& other) = delete;
        Intermidates& operator=(const Intermidates& other) = delete;
        Intermidates(Intermidates&& other) noexcept {
            *this = std::move(other);
        }
    };

   private:
    TgBotApi::Ptr _api;
    const AuthContext* _auth;
    std::vector<KernelConfig> configs;

    Intermidates intermidiates;
    // Recursive: the selection handlers intentionally re-enter while holding
    // this lock (e.g. handle_build -> handle_select -> handle_select_INTERNAL),
    // which would self-deadlock a plain std::mutex (EDEADLK).
    mutable std::recursive_mutex intermidiates_mutex_;
    std::optional<std::string> gitToken;

    // Telegram-free build orchestration (allows dependency injection for
    // testing) plus the presentation-side helpers.
    std::unique_ptr<tgbot::builder::linuxkernel::KernelBuildOrchestrator>
        orchestrator_;
    std::unique_ptr<tgbot::builder::SystemStatsProvider> stats_;
    std::unique_ptr<tgbot::builder::healthcheck::HealthCheckService::Stub>
        healthStub;

    TgBot::InlineKeyboardMarkup::Ptr makeCancelKeyboard() {
        KeyboardBuilder builder;
        builder.addKeyboard(std::make_pair(
            "Cancel Build",
            absl::StrCat(KernelBuildHandler::kCallbackQueryPrefix, "cancel_",
                         intermidiates.build_id.load())));
        return builder.get();
    }

   public:
    constexpr static std::string_view kOutDirectory = "out";
    constexpr static std::string_view kToolchainDirectory = "toolchain";
    constexpr static std::string_view kCallbackQueryPrefix = "kernel_build_";

    // Constructor with dependency injection for testing
    KernelBuildHandler(
        TgBotApi::Ptr api, const CommandLine* line, const AuthContext* auth,
        const ConfigManager* cfgmgr,
        std::shared_ptr<tgbot::builder::linuxkernel::ILinuxKernelBuildService>
            buildServiceImpl,
        std::unique_ptr<SystemMonitorService::Stub> systemMonitorStubImpl,
        std::unique_ptr<tgbot::builder::healthcheck::HealthCheckService::Stub>
            healthStubImpl)
        : _api(api),
          _auth(auth),
          gitToken(cfgmgr->get(ConfigManager::Configs::GITHUB_TOKEN)),
          orchestrator_(std::make_unique<
                        tgbot::builder::linuxkernel::KernelBuildOrchestrator>(
              std::move(buildServiceImpl))),
          stats_(std::make_unique<tgbot::builder::SystemStatsProvider>(
              std::move(systemMonitorStubImpl))),
          healthStub(std::move(healthStubImpl)) {
        loadConfigs(line);
    }

    // Original constructor for production use
    KernelBuildHandler(TgBotApi::Ptr api, const CommandLine* line,
                       const AuthContext* auth, const ConfigManager* cfgmgr)
        : _api(api),
          _auth(auth),
          gitToken(cfgmgr->get(ConfigManager::Configs::GITHUB_TOKEN)) {
        loadConfigs(line);

        auto channel = grpc::CreateChannel(
            *cfgmgr->get(ConfigManager::Configs::KERNELBUILD_SERVER),
            grpc::InsecureChannelCredentials());
        orchestrator_ = std::make_unique<
            tgbot::builder::linuxkernel::KernelBuildOrchestrator>(
            std::make_shared<
                tgbot::builder::linuxkernel::GrpcLinuxKernelBuildService>(
                channel));
        stats_ = std::make_unique<tgbot::builder::SystemStatsProvider>(
            SystemMonitorService::NewStub(channel));
        healthStub =
            tgbot::builder::healthcheck::HealthCheckService::NewStub(channel);

        LOG(INFO) << "Connecting to HealthCheckService...";
        grpc::ClientContext context;
        google::protobuf::Empty request;
        google::protobuf::Empty response;
        auto rc = healthStub->ping(&context, request, &response);
        if (!rc.ok()) {
            throw std::runtime_error(
                "Failed to connect to HealthCheckService: " +
                rc.error_message());
        }
        LOG(INFO) << "Connected to remote successfully";
    }

    template <TgBotApi::ParseMode mode = TgBotApi::ParseMode::None>
    void editQueryMessage(
        const TgBot::CallbackQuery::Ptr& query, const std::string& text,
        const TgBot::InlineKeyboardMarkup::Ptr& replyMarkup = nullptr) {
        if (query->message) {
            if (auto msg = std::get_if<TgBot::Message::Ptr>(&(*query->message))) {
                _api->editMessage<mode>(*msg, text, replyMarkup);
            } else {
                LOG(ERROR) << "Query message is inaccessible, cannot edit message";
            }
        } else {
            LOG(ERROR) << "Query message is null, cannot edit message";
        }
    }

   private:
    void loadConfigs(const CommandLine* line) {
        auto jsonDir =
            line->getPath(FS::PathType::RESOURCES) / "builder" / "kernel";

        std::error_code ec;
        for (const auto it : std::filesystem::directory_iterator(jsonDir, ec)) {
            if (it.path().extension() == ".json") {
                try {
                    configs.emplace_back(it.path());
                } catch (const std::exception& ex) {
                    LOG(ERROR) << ex.what();
                    continue;
                }
            }
        }
        if (ec) {
            LOG(ERROR) << "Failed to opendir for kernel configurations: "
                       << ec.message();
        }
    }

   public:
    constexpr static std::string_view kBuildPrefix = "build_";
    constexpr static std::string_view kSelectPrefix = "select_";
    void start(const Message::Ptr& message) {
        std::lock_guard<std::recursive_mutex> lock(intermidiates_mutex_);
        intermidiates = {};
        if (configs.empty()) {
            _api->sendMessage(message->chat, "No kernel configurations found.");
            return;
        }
        for (auto& config : configs) {
            // Reload config if needed.
            try {
                bool updated = config.reParse();
                if (updated) {
                    // Notify build service about config update
                    if (orchestrator_->updateConfig(config.name,
                                                     config.toJsonString())) {
                        LOG(INFO) << "Reloaded config: " << config.name;
                    }
                }
            } catch (const std::exception& e) {
                LOG(ERROR) << "Failed to reload: " << e.what();
                continue;
            }
        }
        KeyboardBuilder builder;
        for (const auto& config : configs) {
            builder.addKeyboard(std::make_pair(
                "Build " + config.name,
                absl::StrCat(kCallbackQueryPrefix, kBuildPrefix, config.name)));
        }
        _api->sendMessage(message->chat, "Will build kernel...", builder.get());
    }

    void handle_build(const TgBot::CallbackQuery::Ptr& query) {
        std::lock_guard<std::recursive_mutex> lock(intermidiates_mutex_);
        std::string_view data = query->data;
        if (!absl::ConsumePrefix(&data, kBuildPrefix)) {
            return;
        }
        std::string_view kernelName = data;

        for (auto& config : configs) {
            if (config.name == kernelName) {
                intermidiates.current = &config;
                KeyboardBuilder builder;

                if (intermidiates.current->defconfig.devices.size() == 1) {
                    // Only one device, skip selection
                    intermidiates.device =
                        intermidiates.current->defconfig.devices[0];
                    query->data =
                        absl::StrCat(kSelectPrefix, intermidiates.device);
                    DLOG(INFO) << "Only one device, skipping selection";
                    handle_select(query);
                    return;
                }
                for (const auto& device :
                     intermidiates.current->defconfig.devices) {
                    builder.addKeyboard(
                        std::make_pair("Select " + device,
                                       absl::StrCat(kCallbackQueryPrefix,
                                                    kSelectPrefix, device)));
                }
                editQueryMessage(query, "Select device to build",
                                 builder.get());
            }
        }
    }

    constexpr static std::string_view kEnablePrefix = "enable_";
    void handle_select(const TgBot::CallbackQuery::Ptr& query) {
        std::lock_guard<std::recursive_mutex> lock(intermidiates_mutex_);
        std::string_view data = query->data;
        if (!absl::ConsumePrefix(&data, kSelectPrefix)) {
            return;
        }
        std::string_view device = data;
        intermidiates.device = device;

        if (intermidiates.current->fragments.size() == 0) {
            // No fragments, skip selection
            query->data = kContinuePrefix;
            DLOG(INFO) << "No fragments, skipping selection";
            handle_continue(query);
            return;
        }

        KeyboardBuilder builder;
        for (const auto& fragment : intermidiates.current->fragments) {
            intermidiates.fragment_preference[fragment.second.name] =
                fragment.second.default_enabled;
        }
        handle_select_INTERNAL(query);
    }

    constexpr static std::string_view kContinuePrefix = "continue";

    void handle_select_INTERNAL(const TgBot::CallbackQuery::Ptr& query) {
        std::lock_guard<std::recursive_mutex> lock(intermidiates_mutex_);
        KeyboardBuilder builder;
        for (const auto& fragment : intermidiates.current->fragments) {
            bool enabled =
                intermidiates.fragment_preference[std::string(fragment.first)];
            std::string string;
            if (enabled) {
                string = fragment.first + " (Enabled)";
            } else {
                string = fragment.first + " (Disabled)";
            }
            builder.addKeyboard(std::make_pair(
                string, absl::StrCat(kCallbackQueryPrefix, kEnablePrefix,
                                     fragment.second.name)));
        }
        builder.addKeyboard(std::pair{
            "Done", absl::StrCat(kCallbackQueryPrefix, kContinuePrefix)});

        editQueryMessage(query,
                         fmt::format("Device: {}\n\nFragments selection",
                                     intermidiates.device),
                         builder.get());
    }

    void handle_enable(const TgBot::CallbackQuery::Ptr& query) {
        std::lock_guard<std::recursive_mutex> lock(intermidiates_mutex_);
        std::string_view data = query->data;
        if (!absl::ConsumePrefix(&data, kEnablePrefix)) {
            return;
        }
        std::string fragmentName(data);
        intermidiates.fragment_preference[fragmentName] =
            !intermidiates.fragment_preference[fragmentName];
        _api->answerCallbackQuery(
            query->id,
            fmt::format("{}: Now enabled is {}", fragmentName,
                        intermidiates.fragment_preference[fragmentName]));
        handle_select_INTERNAL(query);
    }

    void handle_continue(const TgBot::CallbackQuery::Ptr& query);

    void handle_cancel(const TgBot::CallbackQuery::Ptr& query) {
        if (!orchestrator_->cancel(intermidiates.build_id.load())) {
            LOG(ERROR) << "Error when cancelling build";
            _api->answerCallbackQuery(query->id, "Error when cancelling build");
            return;
        }
        editQueryMessage(query, "Build cancelled!");
        _api->answerCallbackQuery(query->id, "Build cancelled!");
    }

    void handleCallbackQuery(const TgBot::CallbackQuery::Ptr& query) {
        std::string_view data = query->data;

        if (!_auth->isAuthorized(query->from, AuthContext::AccessLevel::AdminUser)) {
            _api->answerCallbackQuery(
                query->id,
                "Sorry son, you are not allowed to touch this keyboard.");
            return;
        }

        if (absl::ConsumePrefix(&data, kBuildPrefix)) {
            // Call the corresponding function
            handle_build(query);
        } else if (absl::ConsumePrefix(&data, kSelectPrefix)) {
            // Call the corresponding function
            handle_select(query);
        } else if (absl::ConsumePrefix(&data, kEnablePrefix)) {
            // Call the corresponding function
            handle_enable(query);
        } else if (absl::ConsumePrefix(&data, kContinuePrefix)) {
            // Call the corresponding function
            handle_continue(query);
        } else if (absl::ConsumePrefix(&data, "cancel_")) {
            // Call the corresponding function
            handle_cancel(query);
        } else {
            LOG(WARNING) << "Unknown query: " << query->data;
        }
    }
};

void KernelBuildHandler::handle_continue(
    const TgBot::CallbackQuery::Ptr& query) {
    using namespace ::tgbot::builder::linuxkernel;

    // Start the build process
    editQueryMessage(query, "Starting build...");
    intermidiates.start_time = std::chrono::system_clock::now();

    // Prepare (make defconfig).
    BuildPrepareRequest request;
    request.set_name(intermidiates.current->name);
    request.set_device_name(intermidiates.device);
    for (const auto& [fragment, enabled] :
         intermidiates.fragment_preference) {
        if (enabled) {
            *request.add_config_fragments() = fragment;
        }
    }
    request.set_clone_depth(1);
    if (gitToken) {
        request.set_github_token(*gitToken);
    }

    LOG(INFO) << "Preparing build for kernel: " << intermidiates.current->name;
    TgKernelObserver prepareObserver(_api, query, stats_.get(),
                                     intermidiates.start_time,
                                     intermidiates.current->name,
                                     intermidiates.device, nullptr);
    auto buildId = orchestrator_->prepare(request, prepareObserver);
    if (!buildId) {
        return;  // Observer already reported the failure.
    }
    LOG(INFO) << "Prepared build with ID: " << *buildId;
    intermidiates.build_id.store(*buildId);
    editQueryMessage(query, "Prepare process completed.");

    // Run the actual build.
    TgKernelObserver buildObserver(_api, query, stats_.get(),
                                   intermidiates.start_time,
                                   intermidiates.current->name,
                                   intermidiates.device, makeCancelKeyboard());
    if (!orchestrator_->runBuild(*buildId, buildObserver)) {
        return;
    }
    editQueryMessage(query, "Build process completed.");

    // Download artifact.
    std::filesystem::path artifactPath;
    TgKernelObserver artifactObserver(_api, query, stats_.get(),
                                      intermidiates.start_time,
                                      intermidiates.current->name,
                                      intermidiates.device, nullptr);
    if (!orchestrator_->downloadArtifact(*buildId, artifactObserver,
                                         &artifactPath)) {
        return;
    }
    editQueryMessage(query, "Artifact retrieved successfully.");

    TgBot::Message::Ptr msg = nullptr;
    if (query->message) {
        if (auto p = std::get_if<TgBot::Message::Ptr>(&(*query->message))) {
            msg = *p;
        }
    }
    if (!msg || !msg->chat) {
        LOG(ERROR) << "Query message is null, cannot send artifact";
        return;
    }
    auto success = _api->sendDocument(
        msg->chat->id,
        InputFile::fromFile(artifactPath, "application/octet-stream"));
    if (success) {
        std::error_code ec;
        std::filesystem::remove(artifactPath, ec);
    }
}

// Define the command handler function
DECLARE_COMMAND_HANDLER(kernelbuild) {
    static std::shared_ptr<KernelBuildHandler> handler;
    static std::once_flag init_flag;
    static std::mutex handler_mutex;
    static bool init_failed = false;

    if (!provider->config->get(ConfigManager::Configs::KERNELBUILD_SERVER)) {
        api->sendMessage(
            message->get<MessageAttrs::Chat>(),
            "Kernel build server is not configured. Please contact the admin.");
        return;
    }

    std::call_once(init_flag, [&]() {
        try {
            handler = std::make_shared<KernelBuildHandler>(
                api, provider->cmdline.get(), provider->auth.get(),
                provider->config.get());

            api->onCallbackQuery(
                "kernelbuild",
                [api, provider](const TgBot::CallbackQuery::Ptr& ptr) {
                    std::string_view data = ptr->data;

                    if (!provider->auth->isAuthorized(
                            ptr->from, AuthContext::AccessLevel::AdminUser)) {
                        api->answerCallbackQuery(
                            ptr->id,
                            "Sorry son, you are not allowed to touch this "
                            "keyboard.");
                        return;
                    }
                    if (!absl::ConsumePrefix(
                            &data, KernelBuildHandler::kCallbackQueryPrefix)) {
                        return;
                    }
                    ptr->data = data;
                    try {
                        std::lock_guard<std::mutex> lock(handler_mutex);
                        if (handler) {
                            handler->handleCallbackQuery(ptr);
                        }
                    } catch (const std::exception& ex) {
                        LOG(ERROR)
                            << "Error handling callback query: " << ex.what();
                    }
                });
        } catch (const std::exception& e) {
            LOG(ERROR) << "Failed to create KernelBuildHandler: " << e.what();
            init_failed = true;
        }
    });

    if (init_failed) {
        api->sendMessage(message->get<MessageAttrs::Chat>(),
                         "Failed to initialize kernel build handler");
        return;
    }

    std::lock_guard<std::mutex> lock(handler_mutex);
    if (handler) {
        handler->start(message->message());
    }
}

extern const struct DynModule cmd_kernelbuild = {
    .flags = DynModule::Flags::Enforced,
    .name = "kernelbuild",
    .description = "Build a kernel, I'm lazy 2",
    .function = COMMAND_HANDLER_NAME(kernelbuild),
};