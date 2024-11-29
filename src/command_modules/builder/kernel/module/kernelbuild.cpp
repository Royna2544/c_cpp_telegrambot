#include <absl/strings/str_cat.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <fmt/chrono.h>
#include <tgbot/TgException.h>
#include <tgbot/types/InlineQueryResultArticle.h>
#include <tgbot/types/InputFile.h>
#include <tgbot/types/InputTextMessageContent.h>
#include <trivial_helpers/_tgbot.h>

#include <Compiler.hpp>
#include <ConfigParsers2.hpp>
#include <ForkAndRun.hpp>
#include <SystemInfo.hpp>
#include <ToolchainConfig.hpp>
#include <ToolchainProvider.hpp>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <archives/Zip.hpp>
#include <chrono>
#include <concepts>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>

#include "Diagnosis.hpp"
#include "ProgressBar.hpp"
#include "api/TgBotApi.hpp"
#include "support/CwdRestorar.hpp"
#include "support/KeyBoardBuilder.hpp"

class KernelBuildHandler {
   public:
    struct Intermidates {
        KernelConfig* current{};
        std::string device;
        std::map<std::string, bool> fragment_preference;
    };

   private:
    TgBotApi::Ptr _api;
    std::vector<KernelConfig> configs;

    Intermidates intermidiates;
    std::filesystem::path kernelDir;

   public:
    constexpr static std::string_view kOutDirectory = "out";
    constexpr static std::string_view kToolchainDirectory = "toolchain";
    constexpr static std::string_view kCallbackQueryPrefix = "kernel_build_";
    KernelBuildHandler(TgBotApi::Ptr api, const CommandLine* line) : _api(api) {
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
        kernelDir = line->getPath(FS::PathType::INSTALL_ROOT) / "kernel_build";
    }

    constexpr static std::string_view kBuildPrefix = "build_";
    constexpr static std::string_view kSelectPrefix = "select_";
    void start(const Message::Ptr& message) {
        if (configs.empty()) {
            _api->sendMessage(message->chat, "No kernel configurations found.");
            return;
        }
        for (auto& config : configs) {
            // Reload config if needed.
            try {
                config.reParse();
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

    void handle_build(TgBot::CallbackQuery::Ptr query) {
        std::string_view data = query->data;
        if (!absl::ConsumePrefix(&data, kBuildPrefix)) {
            return;
        }
        std::string_view kernelName = data;
        for (auto& config : configs) {
            if (config.name == kernelName) {
                intermidiates.current = &config;
                KeyboardBuilder builder;
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
    void handle_select(TgBot::CallbackQuery::Ptr query) {
        std::string_view data = query->data;
        if (!absl::ConsumePrefix(&data, kSelectPrefix)) {
            return;
        }
        std::string_view device = data;
        intermidiates.device = device;

        KeyboardBuilder builder;
        for (const auto& fragment : intermidiates.current->fragments) {
            intermidiates.fragment_preference[fragment.second.name] =
                fragment.second.default_enabled;
        }
        handle_select_INTERNAL(query);
    }

    constexpr static std::string_view kContinuePrefix = "continue";

    void handle_select_INTERNAL(TgBot::CallbackQuery::Ptr query) {
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

    void handle_enable(TgBot::CallbackQuery::Ptr query) {
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

    void handle_continue(TgBot::CallbackQuery::Ptr query);

    template <std::derived_from<toolchains::Provider> T>
    std::optional<Compiler> download(Compiler::Type type) const {
        T toolchain;
        std::error_code ec;
        // Assume bin/ is always valid path for a valid toolchain...
        if (!std::filesystem::is_directory(kernelDir / T::dirname / "bin",
                                           ec)) {
            if (!toolchain.downloadTo(kernelDir / T::dirname)) {
                LOG(ERROR) << "Failed to download toolchain";
                return std::nullopt;
            }
            LOG(INFO) << "Downloaded toolchain";
        } else {
            LOG(INFO) << "Already exists, skip download";
        }
        return Compiler{kernelDir / T::dirname, intermidiates.current->arch,
                        type};
    }

    std::optional<Compiler> download_toolchain() const {
        switch (intermidiates.current->clang) {
            case KernelConfig::ClangSupport::None:
                switch (intermidiates.current->arch) {
                    case KernelConfig::Arch::ARM: {
                        return download<toolchains::GCCAndroidARMProvider>(
                            Compiler::Type::GCCAndroid);
                    }
                    case KernelConfig::Arch::ARM64: {
                        return download<toolchains::GCCAndroidARM64Provider>(
                            Compiler::Type::GCCAndroid);
                    }
                    default:
                        break;
                }
                break;
            case KernelConfig::ClangSupport::Clang:
            case KernelConfig::ClangSupport::FullLLVM:
            case KernelConfig::ClangSupport::FullLLVMWithIAS:
                return download<toolchains::ClangProvider>(
                    Compiler::Type::Clang);
        }
        return std::nullopt;
    }

    void handleCallbackQuery(TgBot::CallbackQuery::Ptr query) {
        std::string_view data = query->data;
        if (absl::ConsumePrefix(&data, kBuildPrefix)) {
            // Call the corresponding function
            handle_build(std::move(query));
        } else if (absl::ConsumePrefix(&data, kSelectPrefix)) {
            // Call the corresponding function
            handle_select(std::move(query));
        } else if (absl::ConsumePrefix(&data, kEnablePrefix)) {
            // Call the corresponding function
            handle_enable(std::move(query));
        } else if (absl::ConsumePrefix(&data, kContinuePrefix)) {
            // Call the corresponding function
            handle_continue(std::move(query));
        } else {
            LOG(WARNING) << "Unknown query: " << query->data;
        }
    }

    static bool link(const std::filesystem::path& path,
                     const std::filesystem::path& created) {
        std::error_code ec;
        std::filesystem::remove(created, ec);
        if (ec) {
            LOG(ERROR) << "Failed to remove existing file: " << ec.message();
            return false;
        }
        std::filesystem::create_directory_symlink(path, created, ec);
        LOG(INFO) << "Create symlink: " << path << " to " << created;
        if (ec) {
            LOG(ERROR) << "Failed to create symlink: " << ec.message();
            return false;
        }
        return true;
    }
};

class ForkAndRunKernel : public ForkAndRun {
    constexpr static std::string_view kInlineQueryKey = "kernelbuild status";
    DeferredExit runFunction() override;
    void handleStderrData(const BufferViewType buffer) override {
        ForkAndRun::handleStderrData(buffer);
        const std::lock_guard<std::mutex> _(output_mutex_);
        output_ << buffer;
    }
    void handleStdoutData(const BufferViewType buffer) override {
        using std::chrono::system_clock;
        if (system_clock::now() - tp > std::chrono::seconds(5)) {
            std::string text = textContent->messageText = fmt::format(
                R"(<blockquote>Start Time: {} (GMT)
Time Spent: {:%M minutes %S seconds}
Kernel Name: {}
Compiler: {}</blockquote>

<blockquote>📱 <b>Device</b>: {}
💻 <b>CPU</b>: {}
💾 <b>Memory</b>: {}</blockquote>

<blockquote>{}</blockquote>)",
                start,
                std::chrono::duration_cast<std::chrono::seconds>(
                    system_clock::now() - start),
                intermidates_->current->name, compiler_.version(),
                intermidates_->device, getPercent<CPUInfo>(),
                getPercent<MemoryInfo>(), buffer);
            try {
                api_->editMessage<TgBotApi::ParseMode::HTML>(message_, text);
            } catch (const TgBot::TgException& e) {
                LOG(ERROR) << "Couldn't parse markdown, with content:";
                LOG(ERROR) << text;
            } catch (const std::exception& e) {
                LOG(ERROR) << "Error while editing message: " << e.what();
            }
            tp = system_clock::now();
        }
        const std::lock_guard<std::mutex> _(output_mutex_);
        output_ << buffer;
    }

    void onExit(int code) override {
        api_->removeInlineQueryKeyboard(kInlineQueryKey);
        ForkAndRun::onExit(code);

        api_->editMessage<TgBotApi::ParseMode::HTML>(
            message_,
            fmt::format(R"(<blockquote>Device: {}
Kernel Name: {}
Took: {:%M minutes %S seconds}
Exit code: {}</blockquote>)",
                        intermidates_->device, intermidates_->current->name,
                        std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now() - start),
                        code));
        if (code != 0) {
            auto tempLog = std::filesystem::temp_directory_path() / "tmp.log";
            if (writeErr(tempLog)) {
                api_->sendDocument(
                    message_->chat,
                    TgBot::InputFile::fromFile(tempLog, "text/plain"),
                    "Output of failed build");
            }
            std::filesystem::remove(tempLog);
            ret = false;
        } else {
            ret = true;
        }

        std::string line;
        while (std::getline(output_, line)) {
            Diagnosis diag(line);
            if (diag.valid) {
                LOG(INFO) << diag.file_path << " has warning " << diag.message;
            }
            UndefinedSym sym(line);
            if (sym.valid) {
                LOG(INFO) << "undefined symbol " << sym.symbol;
            }
        }
    }

    std::vector<std::string> craftArgs() const {
        std::vector<std::string> args;
        args.reserve(11);

        // Make command
        args.emplace_back("make");

        // Output directory
        args.emplace_back(
            fmt::format("O={}", KernelBuildHandler::kOutDirectory));

        // If host architecture is not same, then we are cross compiling
        if (intermidates_->current->arch != KernelConfig::Arch::X86_64) {
            // Setting ARCH= is required
            args.emplace_back(
                fmt::format("ARCH={}", intermidates_->current->arch));

            // Also the CROSS_COMPILE
            args.emplace_back(
                fmt::format("CROSS_COMPILE={}",
                            compiler_.triple(intermidates_->current->arch)));

            // Add CROSS_COMPILE_ARM32 for ARM64 platforms.
            if (intermidates_->current->arch == KernelConfig::Arch::ARM64) {
                args.emplace_back(
                    fmt::format("CROSS_COMPILE_ARM32={}",
                                compiler_.triple(KernelConfig::Arch::ARM)));
            }
        }

        // Adding clang support specific overrides
        for (const auto& [key, value] :
             ToolchainConfig::getVariables(intermidates_->current->clang)) {
            args.emplace_back(fmt::format("{}={}", key, value));
        }
        args.emplace_back(
            fmt::format("-j{}", std::thread::hardware_concurrency()));
        return args;
    }

    std::vector<std::string> craftDefconfigArgs() const {
        std::vector<std::string> defconfigArgs;
        std::map<std::string, std::string> kReplacements = {
            {"{device}", intermidates_->device}};
        defconfigArgs.emplace_back(absl::StrReplaceAll(
            intermidates_->current->defconfig.scheme, kReplacements));
        for (const auto& preference : intermidates_->fragment_preference) {
            if (preference.second) {
                DLOG(INFO) << "Enabled fragment: " << preference.first;
                // Add the fragment to the build arguments
                defconfigArgs.emplace_back(absl::StrReplaceAll(
                    intermidates_->current->fragments[preference.first].scheme,
                    kReplacements));
            }
        }
        return defconfigArgs;
    }

    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point start =
        std::chrono::system_clock::now();

    TgBotApi::Ptr api_;
    Message::Ptr message_;
    std::mutex output_mutex_;
    std::stringstream output_;
    std::filesystem::path kernelPath_;
    std::optional<bool> ret;
    KernelBuildHandler::Intermidates* intermidates_;
    Compiler compiler_;

    void createKeyboard() {
        auto kernelBuild(std::make_shared<TgBot::InlineQueryResultArticle>());
        kernelBuild->title = "Build progress";
        kernelBuild->description = fmt::format(
            "Show the build progress running in chat {}", message_->chat);
        kernelBuild->id = fmt::format("kernelbuild-{}", message_->messageId);
        kernelBuild->inputMessageContent = textContent =
            std::make_shared<TgBot::InputTextMessageContent>();
        // TODO: Don't hardcode this
        kernelBuild->thumbnailUrl =
            "https://raw.githubusercontent.com/Royna2544/c_cpp_telegrambot/"
            "refs/heads/master/resources/photo/build.webp";
        textContent->parseMode =
            TgBotApi::parseModeToStr<TgBotApi::ParseMode::HTML>();
        textContent->messageText = "Not yet ready...";
        api_->addInlineQueryKeyboard(
            TgBotApi::InlineQuery{kInlineQueryKey.data(),
                                  "See the Kernel build progress",
                                  "kernelbuild", false, true},
            kernelBuild);
    }
    TgBot::InputTextMessageContent::Ptr textContent;

   public:
    ForkAndRunKernel(TgBotApi::Ptr api, Message::Ptr message,
                     std::filesystem::path kernelDirectory, Compiler compiler,
                     KernelBuildHandler::Intermidates* intermidates)
        : api_(api),
          message_(std::move(message)),
          kernelPath_(std::move(kernelDirectory)),
          compiler_(std::move(compiler)),
          intermidates_(intermidates) {
        createKeyboard();
    }

    bool writeErr(const std::filesystem::path& where) const {
        std::ofstream file(where);
        if (file.is_open()) {
            file << output_.str();
            file.close();
            return true;
        } else {
            LOG(ERROR) << "Cannot write error output to " << where;
            return false;
        }
    }

    bool result() const { return *ret; }
};

void KernelBuildHandler::handle_continue(TgBot::CallbackQuery::Ptr query) {
    auto compiler = download_toolchain();
    if (!compiler) {
        _api->editMessage(query->message, "Failed to download toolchain");
        return;
    }

    std::error_code ec;
    std::filesystem::path kernelSourceDir =
        kernelDir / intermidiates.current->underscored_name;
    if (!std::filesystem::exists(kernelSourceDir, ec)) {
        if (ec) {
            LOG(ERROR) << "Failed to check repository: " << ec.message();
            _api->editMessage(query->message, "Failed to check repository");
            return;
        }
        _api->editMessage(
            query->message,
            fmt::format("Cloning repository...\nURL: {}\nBranch: {}",
                        intermidiates.current->repo_info.url(),
                        intermidiates.current->repo_info.branch()));
        LOG(INFO) << "Cloning repository...";
        if (!intermidiates.current->repo_info.git_clone(
                kernelSourceDir, intermidiates.current->shallow_clone)) {
            _api->editMessage(query->message, "Failed to clone repository");
            return;
        }
        CwdRestorer cwd(kernelSourceDir);
        if (cwd) {
            for (const auto& patch : intermidiates.current->patches) {
                if (!patch->apply()) {
                    _api->editMessage(query->message,
                                      fmt::format("Failed to apply patch"));
                    return;
                }
            }
        } else {
            LOG(WARNING) << "Cannot push cwd to source directory";
            return;
        }
    }

    if (!link(compiler->path(), kernelSourceDir / kToolchainDirectory)) {
        _api->editMessage(query->message, "Failed to link toolchain dir");
        return;
    }

    ForkAndRunKernel kernel(_api, query->message, kernelSourceDir,
                            std::move(*compiler), &intermidiates);

    _api->editMessage(query->message, "Kernel building...");
    if (!kernel.execute()) {
        _api->editMessage(query->message, "Failed to execute build");
    }
    if (kernel.result() && intermidiates.current->anyKernel.enabled) {
        auto zipname = fmt::format(
            "{}_{}_{:%F}.zip", intermidiates.current->underscored_name,
            intermidiates.device, std::chrono::system_clock::now());
        Zip zip(zipname);
        zip.addDir(kernelSourceDir /
                       intermidiates.current->anyKernel.relative_directory,
                   "");
        zip.addFile(kernelSourceDir / kOutDirectory / "arch" /
                        intermidiates.current->arch / "boot" /
                        intermidiates.current->type,
                    fmt::format("{}", intermidiates.current->type));
        zip.save();
        _api->sendDocument(query->message->chat,
                           TgBot::InputFile::fromFile(zipname, "archive/zip"));
    }
}

DeferredExit ForkAndRunKernel::runFunction() {
    CwdRestorer re(kernelPath_);
    if (!re) {
        LOG(ERROR) << "Failed to push cwd to " << kernelPath_;
        return DeferredExit::generic_fail;
    }
    ForkAndRunShell shell;

    // Extend PATH variable
    shell.env["PATH"] = getenv("PATH");
    shell.env["PATH"] += fmt::format(":{}/{}/bin", kernelPath_.string(),
                                     KernelBuildHandler::kToolchainDirectory);

    // Set the config's environment variables
    for (const auto& [key, value] : intermidates_->current->envMap) {
        shell.env[key] = value;
    }

    // Set custom build host and user
    shell.env["KBUILD_BUILD_HOST"] = "libstdc++";
    shell.env["KBUILD_BUILD_USER"] = "tgbot-builder";

    // Open shell
    if (!shell.open()) {
        LOG(ERROR) << "Cannot open shell!";
    }

    // Exit on error
    shell << "set -e" << ForkAndRunShell::endl;

    if (std::filesystem::exists(".gitmodules")) {
        // Pull submodules
        LOG(INFO) << "Pulling submodules";
        shell << "git submodule update --init" << ForkAndRunShell::endl;
    }

    LOG(INFO) << "Make defconfig...";

    // Make defconfig
    auto args_ = craftArgs();
    std::string defconfigArg = fmt::format(
        "{} {}", fmt::join(args_, " "), fmt::join(craftDefconfigArgs(), " "));
    DLOG(INFO) << defconfigArg;
    shell << defconfigArg << ForkAndRunShell::endl;

    // Build kernel
    LOG(INFO) << "Make kernel...";
    std::string arg = fmt::format("{}", fmt::join(args_, " "));
    DLOG(INFO) << arg;
    shell << arg << ForkAndRunShell::endl;

    return shell.close();
}

// Define the command handler function
DECLARE_COMMAND_HANDLER(kernelbuild) {
    static std::optional<KernelBuildHandler> handler;
    if (!handler) {
        handler.emplace(api, provider->cmdline.get());
        api->onCallbackQuery("kernelbuild", [](TgBot::CallbackQuery::Ptr ptr) {
            std::string_view data = ptr->data;
            if (!absl::ConsumePrefix(
                    &data, KernelBuildHandler::kCallbackQueryPrefix)) {
                return;
            }
            ptr->data = data;
            try {
                handler->handleCallbackQuery(std::move(ptr));
            } catch (const std::exception& ex) {
                LOG(ERROR) << "Error handling callback query: " << ex.what();
            }
        });
    }
    handler->start(message->message());
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::Enforced,
    .name = "kernelbuild",
    .description = "Build a kernel, I'm lazy 2",
    .function = COMMAND_HANDLER_NAME(kernelbuild),
};
