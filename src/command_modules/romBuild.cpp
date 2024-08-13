#include <absl/log/check.h>
#include <absl/strings/match.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>

#include <ConfigParsers.hpp>
#include <PythonClass.hpp>
#include <TgBotWrapper.hpp>
#include <algorithm>
#include <concepts>
#include <filesystem>
#include <initializer_list>
#include <libos/OnTerminateRegistrar.hpp>
#include <libos/libfs.hpp>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <tasks/ROMBuildTask.hpp>
#include <tasks/RepoSyncTask.hpp>
#include <tasks/UploadFileTask.hpp>
#include <utility>

template <typename Impl>
concept canCreateWithApi =
    requires(ApiPtr api, MessagePtr message, PerBuildData data) {
        Impl{api, message, data};
    };
template <typename Impl>
concept canCreateWithData = requires(PerBuildData data) { Impl{data}; };

template <typename Impl>
class TaskWrapperBase {
    PerBuildData::ResultData result{};
    PerBuildData data{};

   protected:
    std::shared_ptr<TgBotApi> api;
    Message::Ptr userMessage;  // User's message
    Message::Ptr sentMessage;  // Message sent by the bot

    bool executeCommon(Impl&& impl) {
        do {
            bool execRes = impl.execute();
            if (!execRes) {
                LOG(ERROR) << "Failed to exec";
                onExecuteFailed();
                return false;
            }
        } while (result.value == PerBuildData::Result::ERROR_NONFATAL);
        onExecuteFinished(result);

        return result.value == PerBuildData::Result::SUCCESS;
    }

   public:
    TaskWrapperBase(const PerBuildData& data, ApiPtr api, MessagePtr message)
        : data(data), api(api), userMessage(message) {
        this->data.result = &result;
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
        api->editMessage(sentMessage, "Failed to execute");
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

    bool execute()
        requires canCreateWithApi<Impl>
    {
        sentMessage = onPreExecute();
        Impl impl(api, sentMessage, data);
        return executeCommon(std::forward<Impl>(impl));
    }
    bool execute()
        requires canCreateWithData<Impl>
    {
        sentMessage = onPreExecute();
        Impl impl(data);
        return executeCommon(std::forward<Impl>(impl));
    }
};

class RepoSync : public TaskWrapperBase<RepoSyncTask> {
    using TaskWrapperBase<RepoSyncTask>::TaskWrapperBase;

    Message::Ptr onPreExecute() override {
        return api->sendReplyMessage(userMessage, "Now syncing...");
    }

    void onExecuteFinished(PerBuildData::ResultData result) override {
        if (result.value == PerBuildData::Result::SUCCESS) {
            api->editMessage(sentMessage, "Repo sync completed successfully");
        } else {
            api->editMessage(sentMessage, "Repo sync failed");
        }
    }
};

class Build : public TaskWrapperBase<RepoSyncTask> {
    using TaskWrapperBase<RepoSyncTask>::TaskWrapperBase;

    Message::Ptr onPreExecute() override {
        return api->sendReplyMessage(userMessage, "Now starting build...");
    }

    void onExecuteFinished(PerBuildData::ResultData result) override {
        std::error_code ec;
        namespace fs = std::filesystem;

        switch (result.value) {
            case PerBuildData::Result::ERROR_FATAL:
                LOG(ERROR) << "Failed to build ROM";
                api->editMessage(sentMessage,
                                 "Build failed:\n" + result.getMessage());
                if (fs::file_size(ROMBuildTask::kErrorLogFile, ec) != 0U) {
                    if (ec) {
                        break;
                    }
                    api->sendDocument(
                        sentMessage->chat->id,
                        TgBot::InputFile::fromFile(
                            ROMBuildTask::kErrorLogFile.data(), "text/plain"));
                }
                break;
            case PerBuildData::Result::SUCCESS:
                api->editMessage(sentMessage, "Build completed successfully");
                break;
        }
    }
};

class Upload : public TaskWrapperBase<UploadFileTask> {
    using TaskWrapperBase<UploadFileTask>::TaskWrapperBase;

    Message::Ptr onPreExecute() override {
        return api->sendReplyMessage(userMessage, "Now uploading...");
    }
    void onExecuteFinished(PerBuildData::ResultData result) override {
        std::string resultText = result.getMessage();
        if (resultText.empty()) {
            resultText = "(Empty result)";
        }
        api->editMessage(sentMessage, resultText);
        if (result.value == PerBuildData::Result::ERROR_FATAL) {
            namespace fs = std::filesystem;
            std::error_code ec;
            if (fs::exists("upload_err.txt", ec)) {
                api->sendDocument(
                    sentMessage->chat->id,
                    TgBot::InputFile::fromFile("upload_err.txt", "text/plain"));
            }
            if (ec) {
                LOG(ERROR) << "Failed to check for upload_err.txt: "
                           << ec.message();
            }
        }
    }
};

namespace {

std::string getSystemInfo() {
    auto py = PythonClass::get();
    auto mod = py->importModule("system_info");
    if (!mod) {
        LOG(ERROR) << "Failed to import system_info module";
        return "";
    }
    auto fn = mod->lookupFunction("get_system_summary");
    if (!fn) {
        LOG(ERROR) << "Failed to find get_system_summary function";
        return "";
    }
    std::string summary;
    if (!fn->call(nullptr, &summary)) {
        LOG(ERROR) << "Failed to call get_system_summary function";
        return "";
    }
    return summary;
}

}  // namespace

template <typename T>
concept isSTLContainer = requires() {
    typename T::iterator;
    typename T::const_iterator;
    { T{}.begin() } -> std::same_as<typename T::iterator>;
    { T{}.end() } -> std::same_as<typename T::iterator>;
    { T{}.size() } -> std::convertible_to<int>;
};

class KeyboardBuilder {
    TgBot::InlineKeyboardMarkup::Ptr keyboard;
    int x = 1;

   public:
    using Button = std::pair<std::string, std::string>;
    using ListOfButton = std::initializer_list<Button>;
    // When we have x, we use that
    explicit KeyboardBuilder(int x) : x(x) {
        keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    }
    // When we don't have x, we assume it is 1, oneline keyboard
    KeyboardBuilder() {
        keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    }

    // Enable method chaining, templated for STL containers
    template <typename ListLike = ListOfButton>
        requires isSTLContainer<ListLike>
    KeyboardBuilder& addKeyboard(const ListLike& list) {
        // We call resize because we know the number of rows and columns
        // If list size has a remainder, we need to add extra row
        // The thing is... appending could take place after the last element,
        // Or in a new row.
        // We will say new row for now...
        // Create a new keyboard
        decltype(keyboard->inlineKeyboard) addingList(
            list.size() / x + (list.size() % x == 0 ? 0 : 1));
        for (auto& row : addingList) {
            row.resize(x);
        }
        int idx = 0;
        // Add buttons to keyboard
        for (const auto& [text, callbackData] : list) {
            auto button = std::make_shared<TgBot::InlineKeyboardButton>();
            button->text = text;
            button->callbackData = callbackData;
            // We can use cool math to fill keyboard in a nice way
            addingList[idx / x][idx % x] = button;
            ++idx;
        }

        // Insert into main keyboard, if any.
        keyboard->inlineKeyboard.insert(keyboard->inlineKeyboard.end(),
                                        addingList.begin(), addingList.end());
        return *this;
    }
    KeyboardBuilder& addKeyboard(const Button& button) {
        return addKeyboard<std::initializer_list<Button>>({button});
    }
    TgBot::InlineKeyboardMarkup::Ptr get() { return keyboard; }
};

class CwdRestorer {
    std::filesystem::path cwd;
    CwdRestorer(const CwdRestorer&) = delete;
    CwdRestorer& operator=(const CwdRestorer&) = delete;
    std::error_code ec;

    void commonErrLog() {
        LOG(ERROR) << "Error while changing cwd: " << ec.message();
    }

   public:
    explicit CwdRestorer(std::filesystem::path newCwd)
        : cwd(std::move(newCwd)) {
        LOG(INFO) << "Changing cwd to: " << cwd.string();
        std::filesystem::current_path(cwd, ec);
        if (!ec) {
            // Successfully changed cwd
            return;
        }

        commonErrLog();
        // If we didn't get ENOENT, then nothing we can do here
        if (ec == std::errc::no_such_file_or_directory) {
            // If it was no such file or directory, then we could try to create
            if (ec = createDirectory(cwd); ec) {
                LOG(ERROR) << "Failed to create build directory: "
                           << ec.message();
            }
        }
    }

    ~CwdRestorer() {
        std::filesystem::current_path(cwd, ec);
        if (ec) {
            commonErrLog();
        }
    }

    operator bool() const noexcept { return !ec; }
};

class ROMBuildQueryHandler {
    bool do_repo_sync = true;
    PerBuildData per_build;
    Message::Ptr sentMessage;
    std::shared_ptr<TgBotApi> api;
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

    template <Buttons... N>
    KeyboardType createKeyboardWith(int x = 1) {
        return KeyboardBuilder(x)
            .addKeyboard({buttonHandlers[static_cast<int>(N)].toButton()...})
            .get();
    }
    template <Buttons N>
    KeyboardBuilder::Button getButtonOf() {
        return buttonHandlers[static_cast<int>(N)].toButton();
    }

   public:
    ROMBuildQueryHandler(std::shared_ptr<TgBotApi> api, MessagePtr userMessage)
        : api(std::move(api)),
          parser(FS::getPathForType(FS::PathType::GIT_ROOT) / "src" /
                 "android_builder" / "configs") {
        settingsKeyboard =
            createKeyboardWith<Buttons::repo_sync, Buttons::back>();
        mainKeyboard =
            createKeyboardWith<Buttons::build_rom, Buttons::send_system_info,
                               Buttons::settings, Buttons::cancel>(2);
        backKeyboard = createKeyboardWith<Buttons::back>();
        sentMessage = this->api->sendMessage(userMessage, "Will build ROM...",
                                             mainKeyboard);
    }
    ~ROMBuildQueryHandler() { api->deleteMessage(sentMessage); }

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

    void handle_repo_sync(const Query& query) {
        do_repo_sync = !do_repo_sync;
        api->answerCallbackQuery(query->id, std::string() +
                                                "Repo sync enabled: " +
                                                (do_repo_sync ? "Yes" : "No"));
    }
    void handle_back(const Query& /*query*/) {
        api->editMessage(sentMessage, "Will build ROM", mainKeyboard);
        per_build.reset();
    }
    void handle_cancel(const Query& /*query*/) {
        api->deleteMessage(sentMessage);
        sentMessage.reset();
    }
    void handle_send_system_info(const Query& query) {
        const static auto info = getSystemInfo();
        api->editMessage(sentMessage, info, backKeyboard);
    }
    void handle_settings(const Query& /*query*/) {
        api->editMessage(sentMessage, "Settings", settingsKeyboard);
    }

    void handle_build(const Query& query) {
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
        api->editMessage(sentMessage, "Select device...", builder.get());
    }

    void handle_confirm(const Query& query) {
        constexpr static std::string_view kBuildDirectory = "rom_build/";
        api->editMessage(sentMessage, "Building...");
        std::error_code ec;
        CwdRestorer cwd(std::filesystem::current_path(ec) / kBuildDirectory);

        if (ec) {
            api->editMessage(sentMessage, "Failed to determine cwd directory");
            return;
        }
        if (!cwd) {
            api->editMessage(sentMessage, "Failed to push cwd");
            return;
        }
        if (do_repo_sync) {
            RepoSync repoSync(per_build, api, sentMessage);
            if (!repoSync.execute()) {
                LOG(INFO) << "RepoSync::execute fails...";
                return;
            }
        }
        Build build(per_build, api, sentMessage);
        if (!build.execute()) {
            LOG(INFO) << "Build::execute fails...";
            return;
        }
        Upload upload(per_build, api, sentMessage);
        if (!upload.execute()) {
            LOG(INFO) << "Upload::execute fails...";
            return;
        }
    }
    void handle_device(const Query& query) {
        std::string_view device = query->data;
        absl::ConsumePrefix(&device, "device_");
        per_build.device = device;
        KeyboardBuilder builder;
        lookup.device = parser.getDevice(device);
        for (const auto& roms : parser.getROMBranches(lookup.device)) {
            builder.addKeyboard(
                {roms->toString(), "rom_" + roms->romInfo->name + "_" +
                                       std::to_string(roms->androidVersion)});
        }
        builder.addKeyboard(getButtonOf<Buttons::back>());
        api->editMessage(sentMessage, "Select ROM...", builder.get());
    }

    void handle_rom(const Query& query) {
        std::string_view rom = query->data;
        absl::ConsumePrefix(&rom, "rom_");
        std::vector<std::string> c = absl::StrSplit(rom, '_');
        CHECK(c.size() == 2);
        const auto romName = c[0];
        // Basically an assert
        const int androidVersion = std::stoi(c[1]);

        lookup.rom = parser.getROMBranches(romName, androidVersion);
        per_build.localManifest =
            parser.getLocalManifest(lookup.rom, lookup.device);

        KeyboardBuilder builder;
        builder.addKeyboard({{"User build", "type_user"},
                             {"Userdebug build", "type_userdebug"},
                             {"Eng build", "type_eng"}});
        api->editMessage(sentMessage, "Select build variant...", builder.get());
    }

    void handle_type(const Query& query) {
        std::string_view type = query->data;
        absl::ConsumePrefix(&type, "type_");
        if (type == "user") {
            per_build.variant = PerBuildData::Variant::kUser;
        } else if (type == "userdebug") {
            per_build.variant = PerBuildData::Variant::kUserDebug;
        } else if (type == "eng") {
            per_build.variant = PerBuildData::Variant::kEng;
        }
        std::stringstream confirm;
        const auto& rom = getValue(per_build.localManifest->rom);

        confirm << "Build variant: " << type << "\n";
        confirm << "Device: " << per_build.device << "\n";
        confirm << "Rom: " << rom->romInfo->name << "\n";
        confirm << "Android version: " << rom->androidVersion << "\n";

        api->editMessage(sentMessage, confirm.str(),
                         createKeyboardWith<Buttons::confirm, Buttons::back>());
    }
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
        confirm
    };

   public:
    void onCallbackQuery(const TgBot::CallbackQuery::Ptr& query) const {
        if (sentMessage == nullptr) {
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
};

std::string operator""_str(const char* str, size_t size) { return {str, size}; }

DECLARE_COMMAND_HANDLER(rombuild, tgWrapper, message) {
    MessageWrapper wrapper(tgWrapper, message);

    static const auto buildRoot =
        FS::getPathForType(FS::PathType::GIT_ROOT) / "src" / "android_builder";
    PythonClass::init();
    PythonClass::get()->addLookupDirectory(buildRoot / "scripts");

    std::shared_ptr<ROMBuildQueryHandler> handler;

    try {
        handler = std::make_shared<ROMBuildQueryHandler>(tgWrapper, message);
    } catch (const std::exception& e) {
        LOG(ERROR) << "Failed to create ROMBuildQueryHandler: " << e.what();
        tgWrapper->sendMessage(
            message, "Failed to initialize ROM build: "_str + e.what());
        return;
    }

    tgWrapper->onCallbackQuery([=](const TgBot::CallbackQuery::Ptr& query) {
        handler->onCallbackQuery(query);
    });
}

DYN_COMMAND_FN(n, module) {
    module.command = "rombuild";
    module.description = "Build a ROM, I'm lazy";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = COMMAND_HANDLER_NAME(rombuild);
    return true;
}
