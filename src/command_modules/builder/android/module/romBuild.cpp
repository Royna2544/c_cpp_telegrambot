#include <absl/log/check.h>
#include <absl/strings/match.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <fmt/format.h>
#include <sys/wait.h>
#include <tgbot/TgException.h>
#include <tgbot/types/ReplyKeyboardRemove.h>

#include <ConfigParsers.hpp>
#include <Zip.hpp>
#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <tasks/ROMBuildTask.hpp>
#include <tasks/RepoSyncTask.hpp>
#include <tasks/UploadFileTask.hpp>
#include <utility>

#include "CurlUtils.hpp"
#include "ForkAndRun.hpp"
#include "SystemInfo.hpp"
#include "support/CwdRestorar.hpp"
#include "support/KeyBoardBuilder.hpp"
#include "utils/CommandLine.hpp"
#include "utils/ConfigManager.hpp"

using std::string_literals::operator""s;

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
        clean_folders,
    };

    ForkAndRun* current{};

   public:
    void setCurrentForkAndRun(ForkAndRun* forkAndRun) { current = forkAndRun; }
    void onCallbackQuery(TgBot::CallbackQuery::Ptr query) const;
};

template <typename Impl>
class TaskWrapperBase {
    PerBuildData::ResultData result{};
    PerBuildData data{};

   protected:
    TgBotApi::Ptr api;
    Message::Ptr userMessage;  // User's message
    Message::Ptr sentMessage;  // Message sent by the bot
    TgBot::InlineKeyboardMarkup::Ptr backKeyboard;
    TgBot::InlineKeyboardMarkup::Ptr cancelKeyboard;
    ROMBuildQueryHandler* queryHandler;

    bool executeCommon(Impl&& impl) {
        queryHandler->setCurrentForkAndRun(&impl);
        do {
            bool execRes = impl.execute();
            if (!execRes || result.value == PerBuildData::Result::NONE) {
                LOG(ERROR) << "The process failed to execute or it didn't "
                              "update result";
                queryHandler->setCurrentForkAndRun(nullptr);
                onExecuteFailed();
                return false;
            }
        } while (result.value == PerBuildData::Result::ERROR_NONFATAL);
        onExecuteFinished(result);
        queryHandler->setCurrentForkAndRun(nullptr);

        return result.value == PerBuildData::Result::SUCCESS;
    }

    void sendFile(const std::string_view filename, bool withReply = false) {
        std::error_code ec;
        if (std::filesystem::file_size(filename, ec) != 0U) {
            if (ec) {
                DLOG(INFO) << "Nonexistent file: " << filename;
                return;
            }
            auto file = TgBot::InputFile::fromFile(filename, "text/plain");
            if (withReply) {
                api->sendReplyDocument(sentMessage, file);
            } else {
                api->sendDocument(sentMessage->chat->id, file);
            }
            if (!std::filesystem::remove(filename, ec)) {
                LOG(ERROR) << "Failed to remove error log file: "
                           << ec.message();
            }
        }
    }

   public:
    TaskWrapperBase(ROMBuildQueryHandler* handler, PerBuildData data,
                    TgBotApi::Ptr api, Message::Ptr message)
        : queryHandler(handler),
          data(std::move(data)),
          api(api),
          userMessage(std::move(message)) {
        this->data.result = &result;
        backKeyboard = KeyboardBuilder()
                           .addKeyboard({{"Back", "back"},
                                         {"Retry", "confirm"},
                                         {"Tick RepoSync", "repo_sync_only"}})
                           .get();
        cancelKeyboard =
            KeyboardBuilder().addKeyboard({{"Cancel", "cancel"}}).get();
    }
    ~TaskWrapperBase() {
        if (sentMessage && queryHandler->pinned()) {
            api->unpinMessage(sentMessage);
        }
    }

    /**
     * @brief Virtual function to be called before the execution of the build
     * process. This function can be overridden in derived classes to perform
     * any necessary pre-execution tasks.
     */
    virtual Message::Ptr onPreExecute() = 0;

    /**
     * @brief Virtual function to be called when the execution of the build
     * process fails. This function can be overridden in derived classes to
     * handle any necessary error handling or recovery.
     */
    virtual void onExecuteFailed() {
        api->editMessage(
            sentMessage,
            "The process failed to execute or it didn't update result");
    }

    /**
     * @brief Virtual function to be called when the execution of the build
     * process finishes. This function can be overridden in derived classes to
     * perform any necessary post-execution tasks.
     *
     * @param result The result of the build process. result.value can be one of
     * the following:
     * - PerBuildData::Result::SUCCESS: The build process completed
     * successfully.
     * - PerBuildData::Result::ERROR_FATAL: The build process failed.
     */
    virtual void onExecuteFinished(PerBuildData::ResultData result) {}

    void preexecute() {
        if (sentMessage && queryHandler->pinned()) {
            api->unpinMessage(sentMessage);
        }
        queryHandler->updateSentMessage(sentMessage = onPreExecute());
        if (queryHandler->pinned()) {
            api->pinMessage(sentMessage);
        }
    }
    bool execute(std::filesystem::path gitAskPassFile)
        requires std::is_same_v<Impl, RepoSyncTask>
    {
        preexecute();
        RepoSyncTask impl(api, sentMessage, data, std::move(gitAskPassFile));
        return executeCommon(std::move(impl));
    }
    bool execute(std::optional<ROMBuildTask::RBEConfig> cfg)
        requires std::is_same_v<Impl, ROMBuildTask>
    {
        preexecute();
        ROMBuildTask impl(api, sentMessage, data, std::move(cfg));
        return executeCommon(std::move(impl));
    }
    bool execute(std::filesystem::path scriptDirectory)
        requires std::is_same_v<Impl, UploadFileTask>
    {
        preexecute();
        UploadFileTask impl(data, std::move(scriptDirectory));
        return executeCommon(std::move(impl));
    }
};

class RepoSync : public TaskWrapperBase<RepoSyncTask> {
    using TaskWrapperBase<RepoSyncTask>::TaskWrapperBase;

    Message::Ptr onPreExecute() override {
        constexpr static std::string_view literal = R"(Now syncing...
Device: {}
ROM: {}
Manifest: {}
)";
        return api->sendReplyMessage(
            userMessage,
            fmt::format(
                literal, queryHandler->builddata().device->toString(),
                queryHandler->builddata().localManifest->rom->toString(),
                queryHandler->builddata().localManifest->name),
            cancelKeyboard);
    }

    void onExecuteFinished(PerBuildData::ResultData result) override {
        if (result.value == PerBuildData::Result::SUCCESS) {
            api->editMessage(sentMessage, "Repo sync completed successfully");
        } else {
            const auto msg =
                api->editMessage(sentMessage, "Repo sync failed", backKeyboard);
            queryHandler->updateSentMessage(msg);
        }
    }

   public:
    virtual ~RepoSync() = default;
};

class Build : public TaskWrapperBase<ROMBuildTask> {
    using TaskWrapperBase<ROMBuildTask>::TaskWrapperBase;

    Message::Ptr onPreExecute() override {
        return api->sendReplyMessage(userMessage, "Now starting build...",
                                     cancelKeyboard);
    }

    void onExecuteFinished(PerBuildData::ResultData result) override {
        std::error_code ec;

        switch (result.value) {
            case PerBuildData::Result::ERROR_FATAL: {
                LOG(ERROR) << "Failed to build ROM";
                std::string message = result.getMessage();
                if (message.empty()) {
                    message = "Failed to build ROM";
                } else {
                    message = fmt::format("Build failed:\n{}", message);
                }
                const auto msg =
                    api->editMessage(sentMessage, message, backKeyboard);
                queryHandler->updateSentMessage(msg);
                for (const auto& file :
                     {ROMBuildTask::kErrorLogFile, ROMBuildTask::kPreLogFile}) {
                    sendFile(file, true);
                }
            } break;
            case PerBuildData::Result::SUCCESS:
                api->editMessage(sentMessage, "Build completed successfully");
                break;
            case PerBuildData::Result::NONE:
                // To reach here, only if the subprocess was killed, is this
                // value possible.
                api->editMessage(sentMessage, "FATAL ERROR");
                break;
            case PerBuildData::Result::ERROR_NONFATAL:
                break;
        }
    }

   public:
    virtual ~Build() = default;
};

class Upload : public TaskWrapperBase<UploadFileTask> {
    using TaskWrapperBase<UploadFileTask>::TaskWrapperBase;

    Message::Ptr onPreExecute() override {
        return api->sendReplyMessage(userMessage, "Now uploading...",
                                     cancelKeyboard);
    }
    void onExecuteFinished(PerBuildData::ResultData result) override {
        std::string resultText = result.getMessage();
        if (resultText.empty()) {
            resultText = "(Empty result)";
        }
        const auto msg =
            api->editMessage(sentMessage, resultText, backKeyboard);
        queryHandler->updateSentMessage(msg);
    }

   public:
    virtual ~Upload() = default;
};

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
    start(std::move(userMessage));
}

void ROMBuildQueryHandler::start(Message::Ptr userMessage) {
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
    if (current != nullptr) {
        current->cancel();
        current = nullptr;
        LOG(INFO) << "User cancelled build";
        handle_back(query);
        _api->answerCallbackQuery(query->id, "Task successfully cancelled!");
    } else {
        _api->deleteMessage(sentMessage);
        sentMessage = nullptr;
    }
}

void ROMBuildQueryHandler::handle_send_system_info(const Query& query) {
    std::stringstream ss;
    ss << SystemSummary();
    _api->editMessage(sentMessage, ss.str(), backKeyboard);
}

void ROMBuildQueryHandler::handle_settings(const Query& /*query*/) {
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
    auto scriptDirectory =
        _commandLine->getPath(FS::PathType::RESOURCES_SCRIPTS);
    auto buildDirectory =
        _commandLine->getPath(FS::PathType::INSTALL_ROOT) / kBuildDirectory;
    auto gitAskFile = scriptDirectory / RepoSyncTask::kGitAskPassFile;
    CwdRestorer cwd(buildDirectory);
    if (!cwd) {
        _api->editMessage(sentMessage, "Failed to push cwd");
        return;
    }
    if (do_repo_sync) {
        RepoSync repoSync(this, per_build, _api, _userMessage);
        if (!repoSync.execute(std::move(gitAskFile))) {
            LOG(INFO) << "RepoSync::execute fails...";
            return;
        }
    }
    std::optional<ROMBuildTask::RBEConfig> config;
    if (do_use_rbe) {
        config.emplace();
        config->baseScript = scriptDirectory / "rbe_env.sh";
        config->api_key =
            _config->get(ConfigManager::Configs::BUILDBUDDY_API_KEY)
                .value_or("");
        LOG_IF(WARNING, config->api_key.empty()) << "Empty API key";
        config->reclientDir = buildDirectory / "rbe_cli";

        if (!std::filesystem::exists(config->reclientDir)) {
            LOG(INFO) << "Downloading reclient...";
            auto zipFile = buildDirectory / "rbe.zip";
            bool ret;
            ret = CURL_download_file(
                "https://github.com/xyz-sundram/Releases/releases/download/"
                "client-linux-amd64/client-linux-amd64.zip",
                zipFile);
            if (!ret) {
                return;
            }
            ret = Zip::extract(zipFile, config->reclientDir);
            if (!ret) {
                return;
            }
        }
    }
    Build build(this, per_build, _api, _userMessage);
    if (!build.execute(config)) {
        LOG(INFO) << "Build::execute fails...";
        return;
    }
    if (do_upload) {
        Upload upload(this, per_build, _api, _userMessage);
        if (!upload.execute(std::move(scriptDirectory))) {
            LOG(INFO) << "Upload::execute fails...";
            return;
        }
    }
    // Success
    _api->editMessageMarkup(sentMessage, nullptr);
    _api->sendMessage(sentMessage->chat, "Build completed");
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
    // So, skip this

    KeyboardBuilder builder;
    for (const auto& roms : roms) {
        builder.addKeyboard(
            {roms->toString(), fmt::format("rom_{}_{}", roms->romInfo->name,
                                           roms->androidVersion)});
    }
    builder.addKeyboard(getButtonOf<Buttons::back>());
    _api->editMessage(sentMessage, "Select ROM...", builder.get());
}

void ROMBuildQueryHandler::handle_rom(const Query& query) {
    std::string_view rom = query->data;
    std::vector<std::string> c = absl::StrSplit(rom, '_');
    CHECK_EQ(c.size(), 2);
    const auto romName = c[0];
    // Basically an assert
    const int androidVersion = std::stoi(c[1]);

    auto [be, en] = std::ranges::remove_if(
        lookup._localManifest, [romName, androidVersion](const auto& manifest) {
            return manifest->rom->romInfo->name != romName ||
                   manifest->rom->androidVersion != androidVersion;
        });
    lookup._localManifest.erase(be, en);

    if (lookup._localManifest.size() != 1) {
        LOG(ERROR) << "Manifest probably contains duplicates, got "
                   << lookup._localManifest.size();
        _api->editMessage(
            sentMessage,
            fmt::format(
                "Failed to assemble local manifest. ROM name: {}, Android {}",
                romName, androidVersion));
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
        per_build.device->codename, rom->romInfo->name, rom->androidVersion);

    _api->editMessage(sentMessage, confirm,
                      createKeyboardWith<Buttons::confirm, Buttons::back>());
}

void ROMBuildQueryHandler::handle_clean_directories(const Query& query) {
    KeyboardBuilder builder;

    std::string_view type = query->data;
    absl::ConsumePrefix(&type, "clean_");
    auto romRootDir =
        _commandLine->getPath(FS::PathType::INSTALL_ROOT) / kBuildDirectory;
    if (type == "rom") {
        LOG(INFO) << "Cleaning directory " << romRootDir;
        _api->answerCallbackQuery(query->id,
                                  "Wait... cleaning may take some time...");
        std::filesystem::remove_all(romRootDir);
    } else if (type == "build") {
        LOG(INFO) << "Cleaning directory " << romRootDir / kOutDirectory;
        _api->answerCallbackQuery(query->id,
                                  "Wait... cleaning may take some time...");
        std::filesystem::remove_all(romRootDir / kOutDirectory);
    }

    std::string entry;
    auto romRootSpace = DiskInfo(romRootDir);

    std::error_code ec;
    entry = "Nothing to clean!";
    if (std::filesystem::exists(romRootDir, ec)) {
        entry = fmt::format("Current disk space free: {:.2f}GB",
                            romRootSpace.availableSpace.value()),
        builder.addKeyboard({"Clean ROM directory", "clean_rom"});
        if (std::filesystem::exists(romRootDir / kOutDirectory, ec)) {
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
            handler.handler(query);
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

    auto gitAskPass =
        provider->cmdline->getPath(FS::PathType::RESOURCES_SCRIPTS) /
        RepoSyncTask::kGitAskPassFile;
    if (auto token =
            provider->config->get(ConfigManager::Configs::GITHUB_TOKEN)) {
        LOG(INFO) << "Create and write git-askpass file";
        std::ofstream ofs(gitAskPass);
        ofs << "echo " << *token << std::endl;
        ofs.close();
        std::error_code ec;
        std::filesystem::permissions(gitAskPass,
                                     std::filesystem::perms::owner_all, ec);
        if (ec) {
            LOG(ERROR) << "Failed to set permissions for git-askpass file: "
                       << ec.message();
            std::filesystem::remove(gitAskPass, ec);
        }
    }

    api->onCallbackQuery("rombuild", [](TgBot::CallbackQuery::Ptr query) {
        if (handler) {
            handler->onCallbackQuery(std::move(query));
        } else {
            LOG(WARNING) << "No ROMBuildQueryHandler to handle callback query";
        }
    });
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::Enforced,
    .name = "rombuild",
    .description = "Build a ROM, I'm lazy",
    .function = COMMAND_HANDLER_NAME(rombuild),
};
