#include <absl/log/check.h>
#include <absl/strings/match.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <google/protobuf/wrappers.pb.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <tgbot/TgException.h>
#include <tgbot/types/ReplyKeyboardRemove.h>

#include <CommandLine.hpp>
#include <ConfigManager.hpp>
#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <libos/libsighandler.hpp>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include "ConfigParsers.hpp"
#include "HealthCheck_service.grpc.pb.h"
#include "ROMBuild_service.grpc.pb.h"
#include "ROMBuild_service.pb.h"
#include "SystemMonitor_service.grpc.pb.h"
#include "SystemMonitor_service.pb.h"
#include "api/RateLimit.hpp"
#include "command_modules/support/CwdRestorar.hpp"
#include "command_modules/support/KeyBoardBuilder.hpp"

using std::string_literals::operator""s;
using namespace tgbot::builder;

class ROMBuildQueryHandler {
    struct {
        bool do_repo_sync = true;
        bool do_upload = true;
        bool didpin = false;
        bool do_use_rbe = false;
    };
    constexpr static std::string_view kBuildDirectory = "rom_build/";
    constexpr static std::string_view kOutDirectory = "out/";
    PerBuildData per_build;
    Message::Ptr sentMessage;
    Message::Ptr _userMessage;
    TgBotApi::Ptr _api;
    CommandLine* _commandLine;
    AuthContext* _auth;
    ConfigManager* _config;
    using KeyboardType = TgBot::InlineKeyboardMarkup::Ptr;

    struct {
        std::vector<ConfigParser::LocalManifest::Ptr> _localManifest;
        std::string codename;
    } lookup;

    KeyboardType settingsKeyboard;
    KeyboardType mainKeyboard;
    KeyboardType backKeyboard;

    enum class Buttons;
    ConfigParser parser;

    // Create keyboard given buttons enum and x length
    template <Buttons... N>
    KeyboardType createKeyboardWith(int x = 1) {
        return KeyboardBuilder(x)
            .addKeyboard({buttonHandlers[static_cast<int>(N)].toButton()...})
            .get();
    }

    // Get button pointed by the enum
    template <Buttons N>
    KeyboardBuilder::Button getButtonOf() {
        return buttonHandlers[static_cast<int>(N)].toButton();
    }

   public:
    ROMBuildQueryHandler(TgBotApi::Ptr api, Message::Ptr userMessage,
                         CommandLine* line, AuthContext* uath,
                         ConfigManager* mgr);

    void updateSentMessage(Message::Ptr message);
    void start(Message::Ptr userMessage);
    bool pinned() const { return didpin; }
    auto builddata() const { return per_build; }

   private:
    using Query = TgBot::CallbackQuery::Ptr;
    struct ButtonHandler {
        std::string text;
        std::string data;
        std::function<void(const Query&)> handler;
        std::function<bool(Query&)> matcher;
        ButtonHandler(std::string text, std::string data,
                      std::function<void(const Query&)> handler,
                      std::function<bool(Query&)> matcher)
            : text(std::move(text)),
              data(std::move(data)),
              handler(std::move(handler)),
              matcher(std::move(matcher)) {}

        ButtonHandler(std::string text, std::string data,
                      std::function<void(const Query&)> handler)
            : text(std::move(text)),
              data(std::move(data)),
              handler(std::move(handler)) {
            matcher = [data = this->data](const Query& query) {
                return query->data == data;
            };
        }

        [[nodiscard]] KeyboardBuilder::Button toButton() const {
            return {text, data};
        }
    };

    // OPTIONS
    // Handle repo sync settings
    void handle_repo_sync_INTERNAL(const Query& query, bool updateSetting);
    void handle_repo_sync_only(const Query& query);
    void handle_repo_sync(const Query& query);
    // Handle upload settings
    void handle_upload(const Query& query);
    // Handle pin message settings
    void handle_pin_message(const Query& query);
    // Handle use rbe settings
    void handle_use_rbe(const Query& query);

    // Handle back button
    void handle_back(const Query& /*query*/);
    // Handle cancel button
    void handle_cancel(const Query& /*query*/);
    // Handle send system info button
    void handle_send_system_info(const Query& query);
    // Handle settings button
    void handle_settings(const Query& /*query*/);
    // Handle build ROM button
    void handle_build(const Query& query);
    // Handle confirm button
    void handle_confirm(const Query& query);
    // Handle device selection button
    void handle_device(const Query& query);
    // Handle ROM selection button
    void handle_rom(const Query& query);
    // Handle Android version selection button
    void handle_android_version(const Query& query);
    // Handle type selection button
    void handle_type(const Query& query);
    // Handle cleaning directories
    void handle_clean_directories(const Query& query);
    // Handle local manifest selection button
    void handle_local_manifest(const Query& query);

#define DECLARE_BUTTON_HANDLER(name, key)                               \
    ButtonHandler {                                                     \
        name, #key, [this](const Query& query) { handle_##key(query); } \
    }
#define DECLARE_BUTTON_HANDLER_WITHPREFIX(name, key, prefix)             \
    ButtonHandler {                                                      \
        name, #key, [this](const Query& query) { handle_##key(query); }, \
            [](Query& query) {                                           \
                std::string_view data = query->data;                     \
                if (absl::ConsumePrefix(&data, prefix)) {                \
                    query->data = data;                                  \
                    return true;                                         \
                }                                                        \
                return false;                                            \
            }                                                            \
    }

    std::vector<ButtonHandler> buttonHandlers = {
        DECLARE_BUTTON_HANDLER("Run repo sync", repo_sync),
        DECLARE_BUTTON_HANDLER("Back", back),
        DECLARE_BUTTON_HANDLER("Cancel", cancel),
        DECLARE_BUTTON_HANDLER("Settings", settings),
        DECLARE_BUTTON_HANDLER("Show system info", send_system_info),
        DECLARE_BUTTON_HANDLER("Build ROM", build),
        DECLARE_BUTTON_HANDLER("Confirm", confirm),
        DECLARE_BUTTON_HANDLER("Do upload", upload),
        DECLARE_BUTTON_HANDLER("Pin the build message", pin_message),
        DECLARE_BUTTON_HANDLER("Use RBE service", use_rbe),
        DECLARE_BUTTON_HANDLER("Run repo sync only", repo_sync_only),
        DECLARE_BUTTON_HANDLER_WITHPREFIX("Clean directories",
                                          clean_directories, "clean_"),
        DECLARE_BUTTON_HANDLER_WITHPREFIX("Select local manifest",
                                          local_manifest, "local_manifest_"),
        DECLARE_BUTTON_HANDLER_WITHPREFIX("Select device", device, "device_"),
        DECLARE_BUTTON_HANDLER_WITHPREFIX("Select ROM", rom, "rom_"),
        DECLARE_BUTTON_HANDLER_WITHPREFIX("Select Android version",
                                          android_version, "android_version_"),
        DECLARE_BUTTON_HANDLER_WITHPREFIX("Select build variant", type,
                                          "type_")};

    enum class Buttons {
        repo_sync,
        back,
        cancel,
        settings,
        send_system_info,
        build_rom,
        confirm,
        upload,
        pin_message,
        use_rbe,
        repo_sync_only,
        clean_folders,
    };

    std::unique_ptr<android::ROMBuildService::Stub> buildStub_;
    std::unique_ptr<system_monitor::SystemMonitorService::Stub> monitorStub_;
    std::unique_ptr<healthcheck::HealthCheckService::Stub> healthStub_;

    class {
        std::mutex m;
        std::string id;
        bool isRunning = false;

       public:
        void start(const std::string& buildId) {
            const std::lock_guard<std::mutex> lock(m);
            id = buildId;
            isRunning = true;
        }
        void finish() {
            const std::lock_guard<std::mutex> lock(m);
            isRunning = false;
        }
        bool running() {
            const std::lock_guard<std::mutex> lock(m);
            return isRunning;
        }
        std::string getId() {
            const std::lock_guard<std::mutex> lock(m);
            return id;
        }
        void setId(const std::string& buildId) {
            const std::lock_guard<std::mutex> lock(m);
            id = buildId;
        }
    } build;

   public:
    void onCallbackQuery(TgBot::CallbackQuery::Ptr query) const;

    // Shutdown method to cancel any running tasks
    void shutdown() {
        if (buildStub_) {
            tgbot::builder::android::BuildAction action;
            grpc::ClientContext context;
            action.set_build_id(build.getId());
            auto rc = buildStub_->CancelBuild(&context, action, nullptr);
            if (!rc.ok()) {
                LOG(ERROR) << "Failed to send cancel build request: "
                           << rc.error_message();
            }
        }
    }
};

namespace {
std::string showPerBuild(const PerBuildData& data,
                         const std::string_view banner) {
    constexpr static std::string_view literal = R"({}

[Build Target Info]
Device: {}
ROM: {}
Android Version: {}
Manifest: {}
)";
    return fmt::format(literal, banner, data.device->toString(),
                       data.localManifest->rom->romInfo->name,
                       data.localManifest->rom->androidVersion->name,
                       data.localManifest->name);
}
}  // namespace

ROMBuildQueryHandler::ROMBuildQueryHandler(TgBotApi::Ptr api,
                                           Message::Ptr userMessage,
                                           CommandLine* line, AuthContext* auth,
                                           ConfigManager* config)
    : _api(api),
      _commandLine(line),
      parser(line->getPath(FS::PathType::RESOURCES) / "builder" / "android"),
      _auth(auth),
      _config(config) {
    settingsKeyboard = createKeyboardWith<Buttons::repo_sync, Buttons::upload,
                                          Buttons::pin_message,
                                          Buttons::use_rbe, Buttons::back>();
    mainKeyboard =
        createKeyboardWith<Buttons::build_rom, Buttons::send_system_info,
                           Buttons::settings, Buttons::cancel,
                           Buttons::clean_folders>(2);
    backKeyboard = createKeyboardWith<Buttons::back>();
    auto channel = grpc::CreateChannel(
        *_config->get(ConfigManager::Configs::KERNELBUILD_SERVER),
        grpc::InsecureChannelCredentials());
    buildStub_ = android::ROMBuildService::NewStub(channel);
    monitorStub_ = system_monitor::SystemMonitorService::NewStub(channel);
    healthStub_ = healthcheck::HealthCheckService::NewStub(channel);

    // Test connection
    grpc::ClientContext context;
    google::protobuf::Empty request;
    google::protobuf::Empty response;
    auto rc = healthStub_->ping(&context, request, &response);
    if (!rc.ok()) {
        LOG(INFO) << "Health check failed: " << rc.error_message();
        throw std::runtime_error("Failed to connect to builder server");
    }
    start(std::move(userMessage));
}

void ROMBuildQueryHandler::start(Message::Ptr userMessage) {
    if (build.running()) {
        _api->sendReplyMessage(userMessage,
                               "No no no. A build is currently running");
        return;
    }
    _userMessage = std::move(userMessage);
    if (sentMessage) {
        try {
            _api->deleteMessage(sentMessage);
        } catch (...) {
            // Ignore
        }
    }
    sentMessage = _api->sendMessage(_userMessage->chat, "Will build ROM...",
                                    mainKeyboard);
    if (didpin) _api->pinMessage(sentMessage);
    per_build.reset();
}

void ROMBuildQueryHandler::updateSentMessage(Message::Ptr message) {
    sentMessage = std::move(message);
}

namespace {
std::string keyToString(const std::string& key, const bool enabled) {
    return fmt::format("{} is now {}", key, enabled ? "enabled" : "disabled");
}
}  // namespace

void ROMBuildQueryHandler::handle_repo_sync(const Query& query) {
    handle_repo_sync_INTERNAL(query, true);
}

void ROMBuildQueryHandler::handle_repo_sync_only(const Query& query) {
    handle_repo_sync_INTERNAL(query, false);
}

void ROMBuildQueryHandler::handle_repo_sync_INTERNAL(const Query& query,
                                                     bool updatesettings) {
    do_repo_sync = !do_repo_sync;
    (void)_api->answerCallbackQuery(query->id,
                                    keyToString("Repo sync", do_repo_sync));
    if (updatesettings) handle_settings(query);
}

void ROMBuildQueryHandler::handle_upload(const Query& query) {
    do_upload = !do_upload;
    (void)_api->answerCallbackQuery(query->id,
                                    keyToString("Uploading", do_upload));
    handle_settings(query);
}

void ROMBuildQueryHandler::handle_use_rbe(const Query& query) {
    if (!_config->get(ConfigManager::Configs::BUILDBUDDY_API_KEY)) {
        (void)_api->answerCallbackQuery(
            query->id, "No BuildBuddy API key set in config!", true);
        return;
    }
    do_use_rbe = !do_use_rbe;
    (void)_api->answerCallbackQuery(query->id,
                                    keyToString("Use RBE", do_use_rbe));
    handle_settings(query);
}

void ROMBuildQueryHandler::handle_pin_message(const Query& query) {
    _api->answerCallbackQuery(query->id, "Trying to pin this message...");
    try {
        _api->pinMessage(sentMessage);
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << "Failed to pin message: " << e.what();
        (void)_api->answerCallbackQuery(query->id, "Failed to pin message");
        return;
    }
    didpin = true;
    _api->answerCallbackQuery(query->id, "Pinned message");
    handle_settings(query);
}

void ROMBuildQueryHandler::handle_back(const Query& /*query*/) {
    _api->editMessage(sentMessage, "Will build ROM", mainKeyboard);
    per_build.reset();
    lookup._localManifest.clear();
    lookup.codename.clear();
}

void ROMBuildQueryHandler::handle_cancel(const Query& query) {
    if (build.running()) {
        shutdown();
        LOG(INFO) << "User cancelled build";
        handle_back(query);
        _api->answerCallbackQuery(query->id, "Task successfully cancelled!");
    } else {
        _api->deleteMessage(sentMessage);
        sentMessage = nullptr;
    }
}

void ROMBuildQueryHandler::handle_send_system_info(const Query& /*query*/) {
    grpc::ClientContext context;
    tgbot::builder::system_monitor::GetSystemInfoRequest request;
    request.set_disk_path("/");
    tgbot::builder::system_monitor::SystemInfo response;
    auto rc = monitorStub_->GetSystemInfo(&context, request, &response);
    if (!rc.ok()) {
        LOG(ERROR) << "Failed to get system info: " << rc.error_message();
        _api->editMessage(sentMessage, "Failed to get system info",
                          backKeyboard);
        return;
    }
    _api->editMessage(
        sentMessage,
        fmt::format(R"(
System Information:
OS: Name={} Version={} KernelVersion={}
CPU: {} ({} cores)
RAM: {} MB
Disk Usage (Of /): {} GB / {} GB total)",
                    response.os_name(), response.os_version(),
                    response.kernel_version(), response.cpu_name(),
                    response.cpu_cores(), response.memory_total_mb(),
                    response.disk_used_gb(), response.disk_total_gb()),
        backKeyboard);
}

void ROMBuildQueryHandler::handle_settings(const Query& /*query*/) {
    grpc::ClientContext context;
    tgbot::builder::android::Settings request;
    request.set_use_rbe_service(do_use_rbe);
    request.set_do_repo_sync(do_repo_sync);
    request.set_do_upload(do_upload);
    request.set_use_ccache(false);
    request.set_do_clean_build(false);
    ::google::protobuf::Empty response;
    auto rc = buildStub_->SetSettings(&context, request, &response);
    if (!rc.ok()) {
        LOG(ERROR) << "Failed to set settings: " << rc.error_message();
        _api->editMessage(sentMessage, "Failed to set settings", backKeyboard);
        return;
    }

    _api->editMessage(sentMessage,
                      fmt::format(R"(Settings

Pinned: {}
RepoSync Enabled: {}
Uploading Enabled: {}
RBE enabled: {})",
                                  didpin, do_repo_sync, do_upload, do_use_rbe),
                      settingsKeyboard);
}

void ROMBuildQueryHandler::handle_confirm(const Query& query) {
    _api->editMessage(sentMessage, "Building...");
    if (didpin) {
        _api->unpinMessage(sentMessage);
    }

    // Start build
    grpc::ClientContext context;
    tgbot::builder::android::BuildRequest request;
    android::BuildSubmission buildSubmission;
    request.set_config_name(per_build.localManifest->name);
    request.set_target_device(per_build.device->codename);
    request.set_rom_name(per_build.localManifest->rom->romInfo->name);
    request.set_rom_android_version(
        per_build.localManifest->rom->androidVersion->version);
    request.set_build_variant(static_cast<android::BuildVariant>(
        static_cast<int>(per_build.variant)));

    auto ret = buildStub_->StartBuild(&context, request, &buildSubmission);
    if (!ret.ok()) {
        LOG(ERROR) << "Failed to start build: " << ret.error_message();
        _api->editMessage(sentMessage, "Failed to start build", backKeyboard);
        return;
    }
    LOG(INFO) << "Started build with ID: " << buildSubmission.build_id();

    grpc::ClientContext logContext;
    android::BuildAction logRequest;
    logRequest.set_build_id(buildSubmission.build_id());
    auto read = buildStub_->StreamLogs(&logContext, logRequest);
    if (!read) {
        LOG(ERROR) << "Failed to stream logs";
        _api->editMessage(sentMessage, "Failed to stream logs", backKeyboard);
        return;
    }
    constexpr auto interval = std::chrono::seconds(5);
    IntervalRateLimiter rateLimiter(1, interval);
    android::BuildLogEntry logEntry;
    while (read->Read(&logEntry)) {
        if (!rateLimiter.check()) {
            continue;  // Skip update if within rate limit
        }
        _api->editMessage(sentMessage, fmt::format("{}", logEntry.message()));
    }

    // Success
    _api->editMessageMarkup(sentMessage, nullptr);
    _api->sendMessage(
        sentMessage->chat,
        showPerBuild(per_build, fmt::format(R"(Build complete!)")));

    if (didpin) {
        try {
            _api->unpinMessage(sentMessage);
        } catch (const TgBot::TgException& e) {
            LOG(ERROR) << "Failed to unpin message: " << e.what();
        }
    }
    sentMessage = nullptr;  // Clear the message out.
}

void ROMBuildQueryHandler::handle_build(const Query& query) {
    KeyboardBuilder builder;
    std::vector<KeyboardBuilder::Button> buttons;
    lookup._localManifest = parser.manifests();

    std::map<std::string, ConfigParser::LocalManifest::Ptr> localManifest;
    for (const auto& manifest : lookup._localManifest) {
        localManifest[manifest->name] = manifest;
    }
    buttons.reserve(localManifest.size());
    for (const auto& [name, manifest] : localManifest) {
        buttons.emplace_back(name, fmt::format("local_manifest_{}", name));
    }
    builder.addKeyboard(buttons);
    builder.addKeyboard(getButtonOf<Buttons::back>());
    _api->editMessage(sentMessage, "Select Local manifest...", builder.get());
}

void ROMBuildQueryHandler::handle_local_manifest(const Query& query) {
    std::string_view name = query->data;
    KeyboardBuilder builder;

    // Erase manifest with different name
    auto [b, e] = std::ranges::remove_if(
        lookup._localManifest,
        [&](const auto& manifest) { return manifest->name != name; });
    lookup._localManifest.erase(b, e);

    // Create a list of devices available for the local manifest.
    std::vector<ConfigParser::Device::Ptr> devices;
    for (const auto& manifest : lookup._localManifest) {
        devices.insert(devices.end(), manifest->devices.begin(),
                       manifest->devices.end());
    }
    // Sort devices
    std::ranges::sort(devices, [](const auto& a, const auto& b) {
        return a->codename < b->codename;
    });

    // Erase duplicates
    auto [be, en] =
        std::ranges::unique(devices, [](const auto& a, const auto& b) {
            return a->codename == b->codename;
        });
    devices.erase(be, en);

    for (const auto& device : devices) {
        builder.addKeyboard(
            {device->toString(), fmt::format("device_{}", device->codename)});
    }
    builder.addKeyboard(getButtonOf<Buttons::back>());
    _api->editMessage(sentMessage, "Select device...", builder.get());
}

void ROMBuildQueryHandler::handle_device(const Query& query) {
    std::string_view devicecodename = lookup.codename = query->data;

    // Erase devices except the selected devices from local manifests
    auto [be, en] = std::ranges::remove_if(
        lookup._localManifest, [devicecodename, this](const auto& manifest) {
            return !std::ranges::any_of(
                manifest->devices, [devicecodename](const auto& device) {
                    return device->codename == devicecodename;
                });
        });
    lookup._localManifest.erase(be, en);

    // Collect ROMs
    std::vector<ConfigParser::ROMBranch::Ptr> roms;
    std::ranges::transform(lookup._localManifest, std::back_inserter(roms),
                           [](const auto& manifest) { return manifest->rom; });

    // Very unlikely that there will be duplicates
    // So, skip that step

    // Find unique ROMs (ignore android version)
    std::vector<ConfigParser::ROMBranch::Ptr> uniqueRoms;
    for (const auto& rom : roms) {
        auto it = std::ranges::find_if(uniqueRoms, [&rom](const auto& urom) {
            return urom->romInfo->name == rom->romInfo->name;
        });
        if (it == uniqueRoms.end()) {
            uniqueRoms.emplace_back(rom);
        }
    }

    KeyboardBuilder builder;
    for (const auto& roms : uniqueRoms) {
        builder.addKeyboard(
            {roms->romInfo->name, fmt::format("rom_{}", roms->romInfo->name)});
    }
    builder.addKeyboard(getButtonOf<Buttons::back>());
    _api->editMessage(sentMessage, "Select ROM...", builder.get());
}

void ROMBuildQueryHandler::handle_rom(const Query& query) {
    std::string_view rom = query->data;

    auto [be, en] = std::ranges::remove_if(
        lookup._localManifest, [rom](const auto& manifest) {
            return manifest->rom->romInfo->name != rom;
        });
    lookup._localManifest.erase(be, en);

    // Collect android versions
    std::vector<ConfigParser::AndroidVersion::Ptr> androidVersions;
    androidVersions.reserve(lookup._localManifest.size());
    for (const auto& manifest : lookup._localManifest) {
        androidVersions.emplace_back(manifest->rom->androidVersion);
    }

    // Find unique android versions
    std::ranges::sort(androidVersions);
    auto [b, e] = std::ranges::unique(androidVersions);
    androidVersions.erase(b, e);

    KeyboardBuilder builder;
    for (const auto& version : androidVersions) {
        builder.addKeyboard({version->name, fmt::format("android_version_{}",
                                                        version->version)});
    }
    builder.addKeyboard(getButtonOf<Buttons::back>());
    _api->editMessage(sentMessage, "Select Android version...", builder.get());
}

void ROMBuildQueryHandler::handle_android_version(const Query& query) {
    int androidVersion = stoi(query->data);

    auto [be, en] = std::ranges::remove_if(
        lookup._localManifest, [androidVersion](const auto& manifest) {
            return manifest->rom->androidVersion->version != androidVersion;
        });
    lookup._localManifest.erase(be, en);

    if (lookup._localManifest.size() != 1) {
        LOG(ERROR) << "Manifest probably contains duplicates, got "
                   << lookup._localManifest.size();
        _api->editMessage(
            sentMessage,
            fmt::format("Failed to assemble local manifest. Found {} manifests",
                        lookup._localManifest.size()));
        return;
    }
    per_build.localManifest = lookup._localManifest.front();
    auto deviceIt = std::ranges::find_if(
        per_build.localManifest->devices, [this](const auto& device) {
            return device->codename == lookup.codename;
        });
    if (deviceIt == per_build.localManifest->devices.end()) {
        LOG(ERROR) << "Device not found in local manifest";
        _api->editMessage(sentMessage, "Failed to assemble local manifest");
        return;
    }
    per_build.device = *deviceIt;
    lookup._localManifest.clear();

    KeyboardBuilder builder;
    builder.addKeyboard({{"User build", "type_user"},
                         {"Userdebug build", "type_userdebug"},
                         {"Eng build", "type_eng"}});
    builder.addKeyboard(getButtonOf<Buttons::back>());
    _api->editMessage(sentMessage, "Select build variant...", builder.get());
}

void ROMBuildQueryHandler::handle_type(const Query& query) {
    std::string_view type = query->data;
    absl::ConsumePrefix(&type, "type_");
    if (type == "user") {
        per_build.variant = PerBuildData::Variant::kUser;
    } else if (type == "userdebug") {
        per_build.variant = PerBuildData::Variant::kUserDebug;
    } else if (type == "eng") {
        per_build.variant = PerBuildData::Variant::kEng;
    }
    const auto& rom = per_build.localManifest->rom;

    const auto confirm = fmt::format(
        "Build variant: {}\nDevice: {}\nRom: {}\nAndroid version: {}", type,
        per_build.device->codename, rom->romInfo->name,
        rom->androidVersion->version);

    _api->editMessage(sentMessage, confirm,
                      createKeyboardWith<Buttons::confirm, Buttons::back>());
}

void ROMBuildQueryHandler::handle_clean_directories(const Query& query) {
    KeyboardBuilder builder;

    tgbot::builder::android::CleanDirectoryRequest cleanDirectoryRequest;
    cleanDirectoryRequest.set_directory_type(
        android::CleanDirectoryType::ROMDirectory);

    std::string_view type = query->data;
    grpc::ClientContext contextClean;
    absl::ConsumePrefix(&type, "clean_");
    if (type == "rom") {
        LOG(INFO) << "Cleaning directory ROM";
        auto r = buildStub_->CleanDirectory(&contextClean,
                                            cleanDirectoryRequest, nullptr);
        if (!r.ok()) {
            LOG(ERROR) << "Failed to clean ROM directory: "
                       << r.error_message();
            _api->editMessage(query->message, "Failed to clean ROM directory",
                              backKeyboard);
            return;
        }
        _api->answerCallbackQuery(query->id,
                                  "Wait... cleaning may take some time...");

    } else if (type == "build") {
        LOG(INFO) << "Cleaning directory ";
        _api->answerCallbackQuery(query->id,
                                  "Wait... cleaning may take some time...");
        cleanDirectoryRequest.set_directory_type(
            android::CleanDirectoryType::BuildDirectory);
        auto r = buildStub_->CleanDirectory(&contextClean,
                                            cleanDirectoryRequest, nullptr);
        if (!r.ok()) {
            LOG(ERROR) << "Failed to clean build directory: "
                       << r.error_message();
            _api->editMessage(query->message, "Failed to clean build directory",
                              backKeyboard);
            return;
        }
        cleanDirectoryRequest.set_directory_type(
            android::CleanDirectoryType::ROMDirectory);
    }
    std::string entry;
    grpc::ClientContext contextDirectoryExists1;
    tgbot::builder::android::DirectoryExistsResponse boolValue;
    auto rpcRes = buildStub_->DirectoryExists(
        &contextDirectoryExists1, cleanDirectoryRequest, &boolValue);
    if (!rpcRes.ok()) {
        LOG(ERROR) << "Failed to check directory existence: "
                   << rpcRes.error_message();
        _api->editMessage(query->message, "Failed to check directory existence",
                          backKeyboard);
        return;
    }

    std::error_code ec;
    entry = "Nothing to clean!";
    if (boolValue.exists()) {
        constexpr double one_upgrader = 1024.0;
        constexpr double gb_upgrader =
            one_upgrader * one_upgrader * one_upgrader;
        grpc::ClientContext contextGetSystemInfo;
        system_monitor::GetSystemInfoRequest getSystemInfoRequest;
        system_monitor::SystemInfo romRootSpace;
        getSystemInfoRequest.set_disk_path("/");
        if (auto res = monitorStub_->GetSystemInfo(
                &contextGetSystemInfo, getSystemInfoRequest, &romRootSpace);
            !res.ok()) {
            LOG(ERROR) << "Failed to get directory size: "
                       << res.error_message();
            _api->editMessage(query->message, "Failed to get directory size",
                              backKeyboard);
            return;
        }
        entry = fmt::format("Current disk space free: {}GB",
                            romRootSpace.disk_used_gb());

        grpc::ClientContext contextDirectoryExists;
        if (auto res = buildStub_->DirectoryExists(
                &contextDirectoryExists, cleanDirectoryRequest, &boolValue);
            !res.ok()) {
            LOG(ERROR) << "Failed to check build directory existence: "
                       << rpcRes.error_message();
            _api->editMessage(query->message,
                              "Failed to check build directory existence",
                              backKeyboard);
            return;
        }
        builder.addKeyboard({"Clean ROM directory", "clean_rom"});
        if (boolValue.exists()) {
            builder.addKeyboard({"Clean ROM build directory", "clean_build"});
        }
    }
    builder.addKeyboard(getButtonOf<Buttons::back>());
    _api->editMessage(query->message, entry, builder.get());
}

void ROMBuildQueryHandler::onCallbackQuery(
    TgBot::CallbackQuery::Ptr query) const {
    if (sentMessage == nullptr) {
        DLOG(INFO) << "No message to handle callback query";
        return;
    }
    if (query->message->chat->id != sentMessage->chat->id ||
        query->message->messageId != sentMessage->messageId) {
        DLOG(INFO) << "Mismatch on message id";
        return;
    }
    if (!_auth->isAuthorized(query->from)) {
        _api->answerCallbackQuery(
            query->id,
            "Sorry son, you are not allowed to touch this keyboard.");
        return;
    }
    for (const auto& handler : buttonHandlers) {
        if (handler.matcher(query)) {
            try {
                handler.handler(query);
            } catch (const std::filesystem::filesystem_error& ex) {
                _api->answerCallbackQuery(
                    query->id, fmt::format("FSException: {}", ex.what()));
            }
            return;
        }
    }
    LOG(ERROR) << "Unknown callback query: " << query->data;
}

DECLARE_COMMAND_HANDLER(rombuild) {
    static std::shared_ptr<ROMBuildQueryHandler> handler;

    if (handler) {
        handler->start(message->message());
        return;
    }

    try {
        handler = std::make_shared<ROMBuildQueryHandler>(
            api, message->message(), provider->cmdline.get(),
            provider->auth.get(), provider->config.get());
    } catch (const std::exception& e) {
        LOG(ERROR) << "Failed to create ROMBuildQueryHandler: " << e.what();
        api->sendMessage(message->get<MessageAttrs::Chat>(),
                         "Failed to initialize ROM build: "s + e.what());
        return;
    }

    api->onCallbackQuery("rombuild", [](TgBot::CallbackQuery::Ptr query) {
        if (handler) {
            handler->onCallbackQuery(std::move(query));
        } else {
            LOG(WARNING) << "No ROMBuildQueryHandler to handle callback query";
        }
    });
}

extern const struct DynModule cmd_rombuild = {
    .flags = DynModule::Flags::Enforced,
    .name = "rombuild",
    .description = "Build a ROM, I'm lazy",
    .function = COMMAND_HANDLER_NAME(rombuild),
};
