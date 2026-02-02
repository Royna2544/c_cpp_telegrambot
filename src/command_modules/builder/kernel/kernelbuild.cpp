#include <absl/strings/str_cat.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <tgbot/TgException.h>
#include <tgbot/types/InlineQueryResultArticle.h>
#include <tgbot/types/InputFile.h>
#include <tgbot/types/InputTextMessageContent.h>
#include <trivial_helpers/_tgbot.h>

#include <ConfigParsers2.hpp>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <unordered_map>
#include <utility>

#include "ConfigManager.hpp"
#include "LinuxKernelBuild_service.grpc.pb.h"
#include "LinuxKernelBuild_service.pb.h"
#include "SystemMonitor_service.grpc.pb.h"
#include "SystemMonitor_service.pb.h"
#include "api/AuthContext.hpp"
#include "api/RateLimit.hpp"
#include "api/TgBotApi.hpp"
#include "command_modules/support/KeyBoardBuilder.hpp"
#include "tgbot/types/InlineKeyboardMarkup.h"

using tgbot::builder::linuxkernel::LinuxKernelBuildService;
using tgbot::builder::system_monitor::SystemMonitorService;

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
    std::filesystem::path kernelDir;
    std::optional<std::string> gitToken;

    // gRPC channel
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<LinuxKernelBuildService::Stub> stub;
    std::unique_ptr<SystemMonitorService::Stub> systemMonitorStub;

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
    KernelBuildHandler(TgBotApi::Ptr api, const CommandLine* line,
                       const AuthContext* auth, const ConfigManager* cfgmgr)
        : _api(api),
          _auth(auth),
          gitToken(cfgmgr->get(ConfigManager::Configs::GITHUB_TOKEN)) {
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
        if (auto it =
                cfgmgr->get(ConfigManager::Configs::FILEPATH_KERNEL_BUILD);
            it) {
            kernelDir = *it;
        } else {
            kernelDir =
                line->getPath(FS::PathType::INSTALL_ROOT) / "kernel_build";
        }

        channel = grpc::CreateChannel(
            *cfgmgr->get(ConfigManager::Configs::KERNELBUILD_SERVER),
            grpc::InsecureChannelCredentials());
        stub = LinuxKernelBuildService::NewStub(channel);
        systemMonitorStub = SystemMonitorService::NewStub(channel);
    }

    constexpr static std::string_view kBuildPrefix = "build_";
    constexpr static std::string_view kSelectPrefix = "select_";
    void start(const Message::Ptr& message) {
        intermidiates = {};
        if (configs.empty()) {
            _api->sendMessage(message->chat, "No kernel configurations found.");
            return;
        }
        for (auto& config : configs) {
            // Reload config if needed.
            try {
                bool updated = config.reParse();
                grpc::ClientContext grpcContext;
                if (updated) {
                    // Notify gRPC server about config update
                    ::tgbot::builder::linuxkernel::ConfigResponse response;
                    auto request = ::tgbot::builder::linuxkernel::Config();
                    request.set_name(config.name);
                    request.set_json_content(config.toJsonString());
                    stub->updateConfig(&grpcContext, request, &response);
                    if (!response.success()) {
                        LOG(ERROR) << "Failed to notify gRPC server about "
                                      "config update: "
                                   << response.message();
                    } else {
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
                _api->editMessage(query->message, "Select device to build",
                                  builder.get());
            }
        }
    }

    constexpr static std::string_view kEnablePrefix = "enable_";
    void handle_select(const TgBot::CallbackQuery::Ptr& query) {
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

        _api->editMessage(query->message,
                          fmt::format("Device: {}\n\nFragments selection",
                                      intermidiates.device),
                          builder.get());
    }

    void handle_enable(const TgBot::CallbackQuery::Ptr& query) {
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
    bool handle_prepare(const TgBot::CallbackQuery::Ptr& query, int* build_id);
    bool handle_build_process(const TgBot::CallbackQuery::Ptr& query,
                              int build_id);

    void handle_cancel(const TgBot::CallbackQuery::Ptr& query) {
        grpc::ClientContext context;
        tgbot::builder::linuxkernel::BuildRequest req;
        tgbot::builder::linuxkernel::BuildStatus resp;
        req.set_build_id(intermidiates.build_id.load());
        auto status = stub->cancelBuild(&context, req, &resp);
        if (!status.ok()) {
            LOG(ERROR) << "gRPC error when cancelling build: "
                       << status.error_message();
            _api->answerCallbackQuery(
                query->id,
                "gRPC error when cancelling build: " + status.error_message());
            return;
        }
        _api->editMessage(query->message, "Build cancelled.");
        _api->answerCallbackQuery(query->id, "Build cancelled!");
    }

    bool handle_artifact_download(const TgBot::CallbackQuery::Ptr& query,
                                  int build_id, std::filesystem::path* outPath);

    void handleCallbackQuery(const TgBot::CallbackQuery::Ptr& query) {
        std::string_view data = query->data;

        if (!_auth->isAuthorized(query->from)) {
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

bool KernelBuildHandler::handle_prepare(const TgBot::CallbackQuery::Ptr& query,
                                        int* build_id) {
    constexpr auto interval = std::chrono::seconds(5);
    IntervalRateLimiter rateLimiter(1, interval);
    using namespace ::tgbot::builder::linuxkernel;

    // Prepare the build process
    // Prepare gRPC request
    BuildPrepareRequest request;
    request.set_name(intermidiates.current->name);
    request.set_device_name(intermidiates.device);
    for (const auto& [fragment, enabled] : intermidiates.fragment_preference) {
        if (enabled) {
            *request.add_config_fragments() = fragment;
        }
    }
    request.set_clone_depth(1);
    if (gitToken) {
        request.set_github_token(*gitToken);
    }

    // Make defconfig
    LOG(INFO) << "Preparing build for kernel: " << intermidiates.current->name;

    BuildStatus response;
    grpc::ClientContext grpcContext;

    intermidiates.start_time = std::chrono::system_clock::now();

    auto status = stub->prepareBuild(&grpcContext, request);
    while (status->Read(&response)) {
        *build_id = response.build_id();
        if (!rateLimiter.check()) {
            continue;  // Skip update if within rate limit
        }
        grpc::ClientContext monitorContext;
        tgbot::builder::system_monitor::SystemStats info;
        auto info_status = systemMonitorStub->GetStats(
            &monitorContext, tgbot::builder::system_monitor::GetStatsRequest{},
            &info);
        auto fmted_msg = fmt::format(
            R"(
<blockquote>Start Time: {} (GMT)
Time Spent: {:%M minutes %S seconds}
Kernel Name: {}</blockquote>
<blockquote>📱 <b>Device</b>: {}
💻 <b>CPU</b>: {:.2f}%
💾 <b>Memory</b>: {}MB / {}MB</blockquote>
<blockquote>{}</blockquote>)",
            intermidiates.start_time,
            std::chrono::system_clock::now() - intermidiates.start_time,
            intermidiates.current->name, intermidiates.device,
            info.cpu_usage_percent(), info.memory_used_mb(),
            info.memory_total_mb(), response.output());
        _api->editMessage<TgBotApi::ParseMode::HTML>(query->message, fmted_msg);
    }
    auto status_finish = status->Finish();
    if (!status_finish.ok()) {
        _api->editMessage(query->message, "Prepare failed due to gRPC error.");
        LOG(ERROR) << "Prepare failed due to gRPC error. Error message: "
                   << status_finish.error_message();
        return false;
    } else {
        _api->editMessage(query->message, "Build process prepared.");
    }
    if (response.status() != ProgressStatus::SUCCESS) {
        _api->editMessage(
            query->message,
            "Prepare incomplete!!, last message: " + response.output());
        LOG(ERROR) << "Prepare incomplete!!, last message: "
                   << response.output();
        return false;
    }
    return true;
}

bool KernelBuildHandler::handle_build_process(
    const TgBot::CallbackQuery::Ptr& query, int build_id) {
    constexpr auto interval = std::chrono::seconds(5);
    IntervalRateLimiter rateLimiter(1, interval);
    using namespace ::tgbot::builder::linuxkernel;

    BuildRequest buildRequest;
    buildRequest.set_build_id(build_id);

    grpc::ClientContext grpcContext_v2;
    auto finalResponse = stub->doBuild(&grpcContext_v2, buildRequest);

    BuildStatus response;
    while (finalResponse->Read(&response)) {
        if (!rateLimiter.check()) {
            continue;  // Skip update if within rate limit
        }

        grpc::ClientContext monitorContext;
        tgbot::builder::system_monitor::SystemStats info;
        auto info_status = systemMonitorStub->GetStats(
            &monitorContext, tgbot::builder::system_monitor::GetStatsRequest{},
            &info);
        auto fmted_msg = fmt::format(
            R"(
<blockquote>Start Time: {} (GMT)
Time Spent: {:%M minutes %S seconds}
Kernel Name: {}</blockquote>
<blockquote>📱 <b>Device</b>: {}
💻 <b>CPU</b>: {:.2f}%
💾 <b>Memory</b>: {}MB / {}MB</blockquote>
<blockquote>{}</blockquote>)",
            intermidiates.start_time,
            std::chrono::system_clock::now() - intermidiates.start_time,
            intermidiates.current->name, intermidiates.device,
            info.cpu_usage_percent(), info.memory_used_mb(),
            info.memory_total_mb(), response.output());
        _api->editMessage<TgBotApi::ParseMode::HTML>(query->message, fmted_msg,
                                                     makeCancelKeyboard());
    }
    auto finish_v2 = finalResponse->Finish();
    if (!finish_v2.ok()) {
        _api->editMessage(query->message, "Build failed due to gRPC error.");
        LOG(ERROR) << "Build failed due to gRPC error. Error message: "
                   << finish_v2.error_message();
    } else {
        _api->editMessage(query->message, "Build process completed.");
    }
    if (response.status() != ProgressStatus::SUCCESS) {
        _api->editMessage(query->message, "Build incomplete!!, last message: " +
                                              response.output());
        LOG(ERROR) << "Build incomplete!!, last message: " << response.output();
        return false;
    }
    return true;
}

bool KernelBuildHandler::handle_artifact_download(
    const TgBot::CallbackQuery::Ptr& query, int build_id,
    std::filesystem::path* outPath) {
    using namespace ::tgbot::builder::linuxkernel;

    BuildRequest artifactRequest;
    artifactRequest.set_build_id(build_id);
    ArtifactChunk response_v2;
    ArtifactMetadata artifactMetadata;
    grpc::ClientContext artifactContext;
    auto artifactResponse =
        stub->getArtifact(&artifactContext, artifactRequest);
    std::ofstream outputFile;
    while (artifactResponse->Read(&response_v2)) {
        if (!outputFile.is_open()) {
            artifactMetadata = response_v2.metadata();
            LOG(INFO) << "Receiving artifact: " << artifactMetadata.filename()
                      << ", size: " << artifactMetadata.total_size();
            outputFile.open(kernelDir / artifactMetadata.filename(),
                            std::ios::binary);
            if (!outputFile.is_open()) {
                LOG(ERROR) << "Failed to open output file: "
                           << kernelDir / artifactMetadata.filename();
                _api->editMessage(query->message,
                                  "Failed to open output file for writing.");
                return false;
            }
        }
        outputFile.write(response_v2.data().data(), response_v2.data().size());
    }
    outputFile.close();
    auto finish_v3 = artifactResponse->Finish();
    if (!finish_v3.ok()) {
        _api->editMessage(query->message, "Failed to retrieve artifact.");
        LOG(ERROR) << "Failed to retrieve artifact. Error message: "
                   << finish_v3.error_message();
        return false;
    } else {
        _api->editMessage(query->message, "Artifact retrieved successfully.");
    }

    LOG(INFO) << "Artifact " << artifactMetadata.filename()
              << " received successfully.";
    *outPath = kernelDir / artifactMetadata.filename();
    return true;
}

void KernelBuildHandler::handle_continue(
    const TgBot::CallbackQuery::Ptr& query) {
    // Start the build process
    _api->editMessage(query->message, "Starting build...");
    int build_id = 0;
    if (!handle_prepare(query, &build_id)) {
        return;
    }
    LOG(INFO) << "Prepared build with ID: " << build_id;
    intermidiates.build_id.store(build_id);
    // Start actual build
    if (!handle_build_process(query, build_id)) {
        // return;
    }
    // Download artifact
    std::filesystem::path artifactPath;
    if (!handle_artifact_download(query, build_id, &artifactPath)) {
        return;
    }
    _api->sendDocument(
        query->message->chat,
        InputFile::fromFile(artifactPath, "application/octet-stream"));
}

// Define the command handler function
DECLARE_COMMAND_HANDLER(kernelbuild) {
    static std::optional<KernelBuildHandler> handler;
    if (!provider->config->get(ConfigManager::Configs::KERNELBUILD_SERVER)) {
        api->sendMessage(
            message->get<MessageAttrs::Chat>(),
            "Kernel build server is not configured. Please contact the admin.");
        return;
    }
    if (!handler) {
        handler.emplace(api, provider->cmdline.get(), provider->auth.get(),
                        provider->config.get());
        api->onCallbackQuery("kernelbuild", [api, provider](
                                                const TgBot::CallbackQuery::Ptr&
                                                    ptr) {
            std::string_view data = ptr->data;

            if (!provider->auth->isAuthorized(ptr->from)) {
                api->answerCallbackQuery(
                    ptr->id,
                    "Sorry son, you are not allowed to touch this keyboard.");
                return;
            }
            if (!absl::ConsumePrefix(
                    &data, KernelBuildHandler::kCallbackQueryPrefix)) {
                return;
            }
            ptr->data = data;
            try {
                handler->handleCallbackQuery(ptr);
            } catch (const std::exception& ex) {
                LOG(ERROR) << "Error handling callback query: " << ex.what();
            }
        });
    }
    handler->start(message->message());
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::Enforced,
    .name = "kernelbuild",
    .description = "Build a kernel, I'm lazy 2",
    .function = COMMAND_HANDLER_NAME(kernelbuild),
};
