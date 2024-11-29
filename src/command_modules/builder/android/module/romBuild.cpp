#include <absl/log/check.h>
#include <absl/strings/match.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <fmt/format.h>
#include <sys/wait.h>
#include <tgbot/TgException.h>
#include <tgbot/types/ReplyKeyboardRemove.h>

#include <ConfigParsers.hpp>
#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <system_error>
#include <tasks/ROMBuildTask.hpp>
#include <tasks/RepoSyncTask.hpp>
#include <tasks/UploadFileTask.hpp>
#include <utility>

#include "ForkAndRun.hpp"
#include "SystemInfo.hpp"
#include "support/CwdRestorar.hpp"
#include "support/KeyBoardBuilder.hpp"
#include "utils/CommandLine.hpp"
#include "utils/ConfigManager.hpp"

template <typename Impl>
concept canCreateWithApi =
    requires(TgBotApi::Ptr api, Message::Ptr message, PerBuildData data) {
        Impl{api, message, data};
    };

template <typename Impl>
concept canCreateWithData = requires(PerBuildData data) { Impl{data}; };

using std::string_literals::operator""s;

class ROMBuildQueryHandler {
    struct {
        bool do_repo_sync = true;
        bool do_upload = true;
        bool didpin = false;
    };
    constexpr static std::string_view kBuildDirectory = "rom_build/";
    constexpr static std::string_view kOutDirectory = "out/";
    PerBuildData per_build;
    Message::Ptr sentMessage;
    Message::Ptr _userMessage;
    TgBotApi::Ptr _api;
    CommandLine* _commandLine;
    using KeyboardType = TgBot::InlineKeyboardMarkup::Ptr;

    struct {
        ConfigParser::ROMBranch::Ptr rom;
        ConfigParser::Device::Ptr device;
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
    bool pinned() const { return didpin; }
    ROMBuildQueryHandler(TgBotApi::Ptr api, Message::Ptr userMessage,
                         CommandLine* line);

    void updateSentMessage(Message::Ptr message);
    void start(Message::Ptr userMessage);

   private:
    using Query = TgBot::CallbackQuery::Ptr;
    struct ButtonHandler {
        std::string text;
        std::string data;
        std::function<void(const Query&)> handler;
        std::function<bool(const Query&)> matcher = [this](const Query& query) {
            return query->data == data;
        };
        ButtonHandler(std::string text, std::string data,
                      std::function<void(const Query&)> handler,
                      std::function<bool(const Query&)> matcher)
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
    void handle_repo_sync(const Query& query);
    // Handle upload settings
    void handle_upload(const Query& query);
    // Handle pin message settings
    void handle_pin_message(const Query& query);

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

#define DECLARE_BUTTON_HANDLER(name, key)                               \
    ButtonHandler {                                                     \
        name, #key, [this](const Query& query) { handle_##key(query); } \
    }
#define DECLARE_BUTTON_HANDLER_WITHPREFIX(name, key, prefix)             \
    ButtonHandler {                                                      \
        name, #key, [this](const Query& query) { handle_##key(query); }, \
            [](const Query& query) {                                     \
                return absl::StartsWith(query->data, prefix);            \
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
        DECLARE_BUTTON_HANDLER_WITHPREFIX("Clean directories",
                                          clean_directories, "clean_"),
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
        clean_folders
    };

    ForkAndRun* current{};

   public:
    void setCurrentForkAndRun(ForkAndRun* forkAndRun) { current = forkAndRun; }
    void onCallbackQuery(const TgBot::CallbackQuery::Ptr& query) const;
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

    void sendFile(const std::string_view filename) {
        std::error_code ec;
        if (std::filesystem::file_size(filename, ec) != 0U) {
            if (ec) {
                DLOG(INFO) << "Nonexistent file: " << filename;
                return;
            }
            api->sendDocument(
                sentMessage->chat->id,
                TgBot::InputFile::fromFile(filename, "text/plain"));
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
        backKeyboard =
            KeyboardBuilder()
                .addKeyboard({{"Back", "back"}, {"Retry", "confirm"}})
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
    bool execute()
        requires std::is_same_v<Impl, ROMBuildTask>
    {
        preexecute();
        ROMBuildTask impl(api, sentMessage, data);
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
        return api->sendReplyMessage(userMessage, "Now syncing...",
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
                    sendFile(file);
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
                                           CommandLine* line)
    : _api(api),
      _commandLine(line),
      parser(line->getPath(FS::PathType::RESOURCES) / "builder" / "android") {
    settingsKeyboard =
        createKeyboardWith<Buttons::repo_sync, Buttons::upload,
                           Buttons::pin_message, Buttons::back>();
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
    do_repo_sync = !do_repo_sync;
    (void)_api->answerCallbackQuery(query->id,
                                    keyToString("Repo sync", do_repo_sync));
}

void ROMBuildQueryHandler::handle_upload(const Query& query) {
    do_upload = !do_upload;
    (void)_api->answerCallbackQuery(query->id,
                                    keyToString("Uploading", do_upload));
}

void ROMBuildQueryHandler::handle_pin_message(const Query& query) {
    (void)_api->answerCallbackQuery(query->id, "Trying to pin this message...");
    try {
        _api->pinMessage(sentMessage);
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << "Failed to pin message: " << e.what();
        (void)_api->answerCallbackQuery(query->id, "Failed to pin message");
        return;
    }
    didpin = true;
    (void)_api->answerCallbackQuery(query->id, "Pinned message");
}

void ROMBuildQueryHandler::handle_back(const Query& /*query*/) {
    _api->editMessage(sentMessage, "Will build ROM", mainKeyboard);
    per_build.reset();
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
    _api->editMessage(sentMessage, "Settings", settingsKeyboard);
}

void ROMBuildQueryHandler::handle_build(const Query& query) {
    KeyboardBuilder builder;
    std::vector<KeyboardBuilder::Button> buttons;
    const auto devices = parser.getDevices();

    std::ranges::transform(
        devices.begin(), devices.end(), std::back_inserter(buttons),
        [&](const auto& device) {
            return std::make_pair(device->toString(),
                                  "device_" + device->codename);
        });
    builder.addKeyboard(buttons);
    builder.addKeyboard(getButtonOf<Buttons::back>());
    _api->editMessage(sentMessage, "Select device...", builder.get());
}

void ROMBuildQueryHandler::handle_confirm(const Query& query) {
    _api->editMessage(sentMessage, "Building...");
    if (didpin) {
        _api->unpinMessage(sentMessage);
    }
    auto scriptDirectory =
        _commandLine->getPath(FS::PathType::RESOURCES_SCRIPTS);
    auto gitAskFile = scriptDirectory / RepoSyncTask::kGitAskPassFile;
    CwdRestorer cwd(_commandLine->getPath(FS::PathType::INSTALL_ROOT) /
                    kBuildDirectory);
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
    Build build(this, per_build, _api, _userMessage);
    if (!build.execute()) {
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
}

void ROMBuildQueryHandler::handle_device(const Query& query) {
    std::string_view device = query->data;
    absl::ConsumePrefix(&device, "device_");
    per_build.device = lookup.device = parser.getDevice(device);
    KeyboardBuilder builder;
    for (const auto& roms : parser.getROMBranches(lookup.device)) {
        builder.addKeyboard(
            {roms->toString(), fmt::format("rom_{}_{}", roms->romInfo->name,
                                           roms->androidVersion)});
    }
    builder.addKeyboard(getButtonOf<Buttons::back>());
    _api->editMessage(sentMessage, "Select ROM...", builder.get());
}

void ROMBuildQueryHandler::handle_rom(const Query& query) {
    std::string_view rom = query->data;
    absl::ConsumePrefix(&rom, "rom_");
    std::vector<std::string> c = absl::StrSplit(rom, '_');
    CHECK_EQ(c.size(), 2);
    const auto romName = c[0];
    // Basically an assert
    const int androidVersion = std::stoi(c[1]);

    lookup.rom = parser.getROMBranches(romName, androidVersion);
    per_build.localManifest =
        parser.getLocalManifest(lookup.rom, lookup.device);
    if (!per_build.localManifest) {
        LOG(ERROR) << "Failed to get local manifest";
        _api->editMessage(
            sentMessage,
            fmt::format(
                "Failed to assemble local manifest. ROM name: {}, Android {}",
                romName, androidVersion));
        return;
    }

    KeyboardBuilder builder;
    builder.addKeyboard({{"User build", "type_user"},
                         {"Userdebug build", "type_userdebug"},
                         {"Eng build", "type_eng"}});
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
        _api->answerCallbackQuery(query->id, "Cleaning ROM directory done.");
    } else if (type == "build") {
        LOG(INFO) << "Cleaning directory " << romRootDir / kOutDirectory;
        _api->answerCallbackQuery(query->id,
                                  "Wait... cleaning may take some time...");
        std::filesystem::remove_all(romRootDir / kOutDirectory);
        _api->answerCallbackQuery(query->id, "Cleaning build directory done.");
    }

    std::string entry;
    auto romRootSpace = DiskInfo(romRootDir);

    std::error_code ec;
    entry = "Clean ROM directory";
    if (!std::filesystem::exists(romRootDir, ec)) {
        entry += " (Nonexistent)";
    }
    builder.addKeyboard({entry, "clean_rom"});
    entry = "Clean build directory";
    if (!std::filesystem::exists(romRootDir / kOutDirectory, ec)) {
        entry += " (Nonexistent)";
    }
    builder.addKeyboard({entry, "clean_build"});
    builder.addKeyboard(getButtonOf<Buttons::back>());
    _api->editMessage(
        query->message,
        fmt::format("Current disk space free: {}", romRootSpace.availableSpace),
        builder.get());
}

void ROMBuildQueryHandler::onCallbackQuery(
    const TgBot::CallbackQuery::Ptr& query) const {
    if (sentMessage == nullptr) {
        DLOG(INFO) << "No message to handle callback query";
        return;
    }
    if (query->message->chat->id != sentMessage->chat->id ||
        query->message->messageId != sentMessage->messageId) {
        DLOG(INFO) << "Mismatch on message id";
        return;
    }
    if (query->from->id != _userMessage->from->id) {
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
            api, message->message(), provider->cmdline.get());
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

    api->onCallbackQuery(
        "rombuild", [](const TgBot::CallbackQuery::Ptr& query) {
            if (handler) {
                handler->onCallbackQuery(query);
            } else {
                LOG(WARNING)
                    << "No ROMBuildQueryHandler to handle callback query";
            }
        });
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::Enforced,
    .name = "rombuild",
    .description = "Build a ROM, I'm lazy",
    .function = COMMAND_HANDLER_NAME(rombuild),
};