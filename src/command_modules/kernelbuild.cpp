#include <absl/strings/str_cat.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <dirent.h>
#include <fcntl.h>
#include <fmt/chrono.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <zip.h>

#include <Compiler.hpp>
#include <ConfigParsers2.hpp>
#include <ForkAndRun.hpp>
#include <StructF.hpp>
#include <ToolchainConfig.hpp>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <thread>
#include <utility>

#include "api/TgBotApi.hpp"
#include "support/CwdRestorar.hpp"
#include "support/KeyBoardBuilder.hpp"
#include "tgbot/types/InputFile.h"
#include "trivial_helpers/raii.hpp"

class Zip {
    zip_t* zip = nullptr;
    std::vector<std::pair<void*, size_t>> mappedFiles;

   public:
    Zip(const Zip&) = delete;
    Zip& operator=(const Zip&) = delete;

    ~Zip() {
        if (zip != nullptr) {
            save();
        }
    }
    explicit Zip(const std::filesystem::path& path) { open(path); }
    Zip() = default;

    bool open(const std::filesystem::path& path) {
        // Create a new zip archive
        int err = 0;
        zip = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
        if (zip == nullptr) {
            std::array<char, 1024> errbuf{};
            zip_error_to_str(errbuf.data(), errbuf.size(), err, errno);
            LOG(ERROR) << fmt::format("Cannot open zip archive: {}",
                                      std::string(errbuf.data()));
            return false;
        }
        return true;
    }

    bool addFile(const std::filesystem::path& filepath,
                 const std::string_view entryname) {
        // Open the file
        auto fd = RAII2<int>::create<int>(
            ::open(filepath.string().c_str(), O_RDONLY), &close);
        if (*fd == -1) {
            PLOG(ERROR) << "Couldn't open file " << filepath;
            fd.reset();
            return false;
        }

        // Get the file size
        struct stat sb {};
        if (fstat(*fd, &sb) == -1) {
            PLOG(ERROR) << "Failed to stat file " << filepath;
            return false;
        }
        size_t filesize = sb.st_size;
        if (filesize == 0) {
            DLOG(INFO) << "Skipping file " << filepath;
            return true;
        }

        // Memory map the file
        void* mapped = mmap(nullptr, filesize, PROT_READ, MAP_PRIVATE, *fd, 0);
        if (mapped == MAP_FAILED) {
            PLOG(ERROR) << "Failed to map file " << filepath;
            return false;
        }

        // Add file to the zip archive
        zip_source_t* source = zip_source_buffer(zip, mapped, filesize, 0);
        if (source == nullptr ||
            zip_file_add(zip, entryname.data(), source, ZIP_FL_OVERWRITE) < 0) {
            LOG(ERROR) << "Cannot add file " << entryname
                       << " to zip: " << zip_strerror(zip);
            zip_source_free(source);
        }
        mappedFiles.emplace_back(mapped, filesize);
        return true;
    }

    bool addDir(const std::filesystem::path& dirpath,
                const std::filesystem::path& ziproot) {
        std::error_code ec;
        for (const auto& entry :
             std::filesystem::directory_iterator(dirpath, ec)) {
            const auto zipentry = fmt::format("{}/{}", ziproot.string(),
                                              entry.path().filename().string());
            if (entry.is_directory()) {
                addDir(entry.path(), zipentry);
            } else if (entry.is_regular_file()) {
                addFile(entry.path(), zipentry);
            } else {
                LOG(WARNING) << "Skipping non-regular file: " << entry.path();
            }
        }
        if (ec) {
            LOG(ERROR) << "Error while iterating over directory: "
                       << ec.message();
            return false;
        }
        return true;
    }

    bool save() {
        if (zip == nullptr) {
            LOG(ERROR) << "Cannot save zip archive, it's already closed";
            return false;
        }
        int err = zip_close(zip);
        if (err != 0) {
            LOG(ERROR) << "Failed to close zip archive: " << zip_strerror(zip);
            return false;
        }
        for (const auto& [memory, size] : mappedFiles) {
            int ret = munmap(memory, size);
            if (ret != 0) {
                PLOG(ERROR) << "Failed to unmap memory";
            }
        }
        zip = nullptr;
        return true;
    }
};

class ForkAndRunKernel : public ForkAndRun {
    DeferredExit runFunction() override;
    void handleStderrData(const BufferViewType buffer) override {
        ForkAndRun::handleStderrData(buffer);
        errorOutput_ << buffer;
    }
    void handleStdoutData(const BufferViewType buffer) override {
        using std::chrono::system_clock;
        if (system_clock::now() - tp > std::chrono::seconds(5)) {
            api_->editMessage<TgBotApi::ParseMode::HTML>(
                message_,
                fmt::format(R"(<blockquote>Start Time: {} (GMT)
Time Spent: {:%M minutes %S seconds}</blockquote>

<blockquote>{}</blockquote>)",
                            start,
                            std::chrono::duration_cast<std::chrono::seconds>(
                                system_clock::now() - start),
                            buffer));
            tp = system_clock::now();
        }
    }

    void onExit(int code) override {
        ForkAndRun::onExit(code);

        api_->editMessage(message_, fmt::format("Exit code: {}", code));
        if (code != 0) {
            auto tempLog = std::filesystem::temp_directory_path() / "tmp.log";
            if (writeErr(tempLog)) {
                api_->sendDocument(
                    message_->chat,
                    TgBot::InputFile::fromFile(tempLog, "text/plain"),
                    "Stderr log");
            }
            std::filesystem::remove(tempLog);
            ret = false;
        } else {
            ret = true;
        }
    }

    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point start =
        std::chrono::system_clock::now();
    std::vector<std::string> args_;
    std::vector<std::string> defconfigArgs_;
    TgBotApi::CPtr api_;
    Message::Ptr message_;
    std::stringstream errorOutput_;
    std::filesystem::path kernelPath_;
    std::optional<bool> ret;

   public:
    ForkAndRunKernel(TgBotApi::CPtr api, Message::Ptr message,
                     std::filesystem::path kernelDirectory,
                     std::vector<std::string> args,
                     std::vector<std::string> defconfigArgs)
        : args_(std::move(args)),
          defconfigArgs_(std::move(defconfigArgs)),
          api_(api),
          message_(std::move(message)),
          kernelPath_(std::move(kernelDirectory)) {}

    bool writeErr(const std::filesystem::path& where) const {
        std::ofstream file(where);
        if (file.is_open()) {
            file << errorOutput_.str();
            file.close();
            return true;
        } else {
            LOG(ERROR) << "Cannot write error output to " << where;
            return false;
        }
    }

    bool result() const { return *ret; }
};

class KernelBuildHandler {
    TgBotApi::CPtr _api;
    std::vector<KernelConfig> configs;

    struct {
        KernelConfig* current{};
        std::string device;
        std::map<std::string, bool> fragment_preference;
    } intermidiates;
    std::filesystem::path kernelDir;

   public:
    constexpr static std::string_view kOutDirectory = "out";
    constexpr static std::string_view kToolchainDirectory = "toolchain";
    constexpr static std::string_view kCallbackQueryPrefix = "kernel_build_";
    KernelBuildHandler(TgBotApi::CPtr api, const CommandLine* line)
        : _api(api) {
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
        kernelDir = line->getPath(FS::PathType::RESOURCES) / "kernel_build";
    }

    constexpr static std::string_view kBuildPrefix = "build_";
    void start(const Message::Ptr& message) {
        if (configs.empty()) {
            _api->sendMessage(message->chat, "No kernel configurations found.");
            return;
        }
        KeyboardBuilder builder;
        for (const auto& config : configs) {
            builder.addKeyboard(std::make_pair(
                "Build " + config.name,
                absl::StrCat(kCallbackQueryPrefix, kBuildPrefix, config.name)));
        }
        _api->sendMessage(message->chat, "Will build kernel...", builder.get());
    }

    constexpr static std::string_view kSelectPrefix = "select_";
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

    void handle_continue(TgBot::CallbackQuery::Ptr query) {
        std::vector<std::string> args{"make",
                                      fmt::format("O={}", kOutDirectory)};
        std::vector<std::string> defconfigArgs;
        std::string arch = "ARCH=";
        switch (intermidiates.current->arch) {
            case KernelConfig::Arch::ARM:
                arch += "arm";
                break;
            case KernelConfig::Arch::ARM64:
                arch += "arm64";
                break;
            case KernelConfig::Arch::X86:
                arch += "x86";
                break;
            case KernelConfig::Arch::X86_64:
                arch += "x86_64";
                break;
        }
        args.push_back(arch);
        Compiler compiler("clang");
        LOG(INFO) << "Compiler version: " << compiler.version();
        if (intermidiates.current->arch != KernelConfig::Arch::X86_64) {
            args.emplace_back(
                fmt::format("CROSS_COMPILE={}",
                            compiler.getTriple(intermidiates.current->arch)));
        }
        for (const auto& [key, value] :
             ToolchainConfig::getVariables(intermidiates.current->clang)) {
            args.emplace_back(fmt::format("{}={}", key, value));
        }
        defconfigArgs.emplace_back(
            absl::StrReplaceAll(intermidiates.current->defconfig.scheme,
                                {{"{device}", intermidiates.device}}));
        for (const auto& preference : intermidiates.fragment_preference) {
            if (preference.second) {
                DLOG(INFO) << "Enabled fragment: " << preference.first;
                // Add the fragment to the build arguments
                defconfigArgs.emplace_back(absl::StrReplaceAll(
                    intermidiates.current->fragments[preference.first].scheme,
                    {{"{device}", intermidiates.device}}));
            }
        }
        args.emplace_back(
            fmt::format("-j{}", std::thread::hardware_concurrency()));
        std::filesystem::path kernelDir =
            "/home/royna/universal_android_kernelbuilder/Eureka_Kernel";

        ForkAndRunKernel kernel(_api, query->message, kernelDir,
                                std::move(args), std::move(defconfigArgs));

        _api->editMessage(query->message, "Kernel building...");
        if (!kernel.execute()) {
            _api->editMessage(query->message, "Failed to execute build");
        }
        if (kernel.result() && intermidiates.current->anyKernel.enabled) {
            Zip zip("my_amazin_zip");
            zip.addDir(
                kernelDir / intermidiates.current->anyKernel.relative_directory,
                "");
            zip.save();
            _api->sendDocument(
                query->message->chat,
                TgBot::InputFile::fromFile("my_amazin_zip", "archive/zip"));
        }
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
};

DeferredExit ForkAndRunKernel::runFunction() {
    CwdRestorer re(kernelPath_);
    if (!re) {
        LOG(ERROR) << "Failed to push cwd to " << kernelPath_;
        return DeferredExit::generic_fail;
    }
    ForkAndRunShell shell;
    shell.env["PATH"] = getenv("PATH");
    shell.env["PATH"] += fmt::format(":{}/{}/bin", kernelPath_.string(),
                                     KernelBuildHandler::kToolchainDirectory);
    if (!shell.open()) {
        LOG(ERROR) << "Cannot open shell!";
    }
    shell << "set -e" << ForkAndRunShell::endl;
    LOG(INFO) << "Make defconfig...";
    shell << fmt::format("{} {}", fmt::join(args_, " "),
                         fmt::join(defconfigArgs_, " "))
          << ForkAndRunShell::endl;
    LOG(INFO) << "Make kernel...";
    shell << fmt::format("{}", fmt::join(args_, " ")) << ForkAndRunShell::endl;
    LOG(INFO) << "Build done!";
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
    .function = COMMAND_HANDLER_NAME(kernelbuild)};
