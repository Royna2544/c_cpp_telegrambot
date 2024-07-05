#include <BotReplyMessage.h>

#include <ConfigParsers.hpp>
#include <MessageWrapper.hpp>
#include <filesystem>
#include <fstream>
#include <libos/libfs.hpp>
#include <string>
#include <system_error>
#include <tasks/ROMBuildTask.hpp>
#include <tasks/RepoSyncTask.hpp>
#include <tasks/UploadFileTask.hpp>

#include "PythonClass.hpp"
#include "cmd_dynamic.h"
#include "command_modules/CommandModule.h"
#include "tasks/PerBuildData.hpp"

namespace {

void sendSystemInfo(MessageWrapper& wrapper, const PerBuildData& data) {
    auto py = PythonClass::get();
    py->addLookupDirectory(data.scriptDirectory);
    auto mod = py->importModule("system_info");
    if (!mod) {
        LOG(ERROR) << "Failed to import system_info module";
        return;
    }
    auto fn = mod->lookupFunction("get_system_summary");
    if (!fn) {
        LOG(ERROR) << "Failed to find get_system_summary function";
        return;
    }
    std::string summary;
    if (!fn->call(nullptr, &summary)) {
        LOG(ERROR) << "Failed to call get_system_summary function";
        return;
    }
    bot_sendMessage(wrapper.getBot(), wrapper.getChatId(), summary);
}

bool parseConfigAndGetTarget(PerBuildData* data, MessageWrapper& wrapper,
                             const std::string& targetName) {
    static auto buildRoot = FS::getPathForType(FS::PathType::GIT_ROOT) / "src" /
                            "android_builder" / "configs";

    const auto romsConfig =
        parse<RomConfig>(std::ifstream(buildRoot / "rom_config.txt"));
    const auto buildConfig =
        parse<BuildConfig>(std::ifstream(buildRoot / "build_config.txt"));

    LOG(INFO) << "Parsed rom config count: " << romsConfig.size();
    LOG(INFO) << "Target: " << targetName;

    // Lookup corresponding buildconfig for the target string
    const auto build = std::ranges::find_if(
        buildConfig,
        [&targetName](const auto& r) { return r.name == targetName; });
    if (build == buildConfig.end()) {
        LOG(ERROR) << "No build config found for target: " << targetName;
        wrapper.sendMessageOnExit("No build config found for target");
        return false;
    }

    // Lookup corresponding rom config for the target config
    const auto rom = std::ranges::find_if(romsConfig, [&build](const auto& r) {
        return r.name == build->romName;
    });
    if (rom == romsConfig.end()) {
        LOG(ERROR) << "No rom config found for build config: "
                   << build->romName;
        wrapper.sendMessageOnExit("No rom config found for build config");
        return false;
    }
    data->rConfig = *rom;
    data->bConfig = *build;
    data->scriptDirectory = buildRoot.parent_path() / "scripts";
    return true;
}

bool repoSync(PerBuildData data, MessageWrapper& wrapper) {
    PerBuildData::ResultData result{};
    data.result = &result;
    RepoSyncTask repoSync(data);
    
    do {
        bool execRes = repoSync.execute();
        if (!execRes) {
            LOG(ERROR) << "Failed to exec";
            wrapper.sendMessageOnExit(
                "Repo sync failed: Couldn't start process");
            return false;
        }
    } while (result.value == PerBuildData::Result::ERROR_NONFATAL);

    switch (result.value) {
        case PerBuildData::Result::SUCCESS:
            wrapper.sendMessageOnExit("Repo sync completed successfully");
            return true;
        case PerBuildData::Result::ERROR_FATAL:
            LOG(ERROR) << "Repo sync failed";
            wrapper.sendMessageOnExit("Repo sync failed\n:" + result.msg);
            break;
        default:
            break;
    }
    return false;
}

bool build(PerBuildData data, MessageWrapper& wrapper) {
    static const std::filesystem::path kErrorLogFile = "out/error.log";
    std::error_code ec;
    const auto api = wrapper.getBot().getApi();
    PerBuildData::ResultData result{};

    data.result = &result;
    auto msg = api.sendMessage(wrapper.getChatId(), "Starting build...");
    wrapper.switchToReplyToMessage();

    ROMBuildTask romBuild(wrapper.getBot(), msg, data);

    do {
        bool execRes = romBuild.execute();
        if (!execRes) {
            LOG(ERROR) << "Failed to exec";
            wrapper.sendMessageOnExit(
                "Build failed: Couldn't start build process");
            return false;
        }
    } while (result.value == PerBuildData::Result::ERROR_NONFATAL);

    switch (result.value) {
        case PerBuildData::Result::ERROR_FATAL:
            LOG(ERROR) << "Failed to build ROM";
            wrapper.sendMessageOnExit("Build failed:\n" + result.msg);
            if (std::filesystem::file_size(kErrorLogFile, ec) != 0U) {
                api.sendDocument(
                    wrapper.getMessage()->chat->id,
                    TgBot::InputFile::fromFile(kErrorLogFile, "text/plain"));
            } else {
                api.sendMessage(wrapper.getMessage()->chat->id,
                                "Could not send " + kErrorLogFile.string() +
                                    ": " + ec.message());
            }
            break;
        case PerBuildData::Result::SUCCESS:
            wrapper.sendMessageOnExit("Build completed successfully");
            return true;
        case PerBuildData::Result::ERROR_NONFATAL:
            break;
    }
    return false;
}
}  // namespace

static void romBuildCommand(const Bot& bot, const Message::Ptr message) {
    PerBuildData PBData;
    MessageWrapper wrapper(bot, message);
    std::error_code ec;
    constexpr static std::string_view kBuildDirectory = "rom_build/";
    const auto cwd = std::filesystem::current_path();
    const auto returnToCwd = [cwd]() {
        std::error_code _ec;
        std::filesystem::current_path(cwd, _ec);
        if (_ec) {
            LOG(ERROR) << "Failed to restore cwd: " << _ec.message();
        }
    };

    if (!wrapper.hasExtraText()) {
        wrapper.sendMessageOnExit("Please provide a target string");
        return;
    }
    // Parse the config files and get the target build config and rom config
    if (!parseConfigAndGetTarget(&PBData, wrapper, wrapper.getExtraText())) {
        return;
    }
    // Send system information about the system
    sendSystemInfo(wrapper, PBData);
    // Send a message to notify about the start of the build process
    bot_sendReplyMessage(bot, message, "Now syncing...");

    std::filesystem::create_directory(kBuildDirectory, ec);
    if (ec && ec != std::make_error_code(std::errc::file_exists)) {
        LOG(ERROR) << "Failed to create build directory: " << ec.message();
        wrapper.sendMessageOnExit("Failed to create build directory");
        return;
    }
    std::filesystem::current_path(kBuildDirectory, ec);
    
    // Sync the repository
    if (!repoSync(PBData, wrapper)) {
        returnToCwd();
        return;
    }
    
    // Build the ROM
    if (!build(PBData, wrapper)) {
        returnToCwd();
        return;
    }

#if 0
    bot_sendMessage(bot, wrapper.getChatId(), "Will upload");

    bool uploadResult = false;
    PBData.result = &uploadResult;
    UploadFileTask uploadTask(wrapper, PBData);
    uploadTask.execute();
#endif
    returnToCwd();
}

extern "C" {
void DYN_COMMAND_SYM(CommandModule& module) {
    module.command = "rombuild";
    module.description = "Build a ROM, I'm lazy";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = romBuildCommand;
}
}