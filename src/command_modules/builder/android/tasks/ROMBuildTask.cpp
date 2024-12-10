#include "ROMBuildTask.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>
#include <tgbot/types/InlineQueryResultArticle.h>
#include <trivial_helpers/_tgbot.h>

#include <ConfigParsers.hpp>
#include <Progress.hpp>
#include <SystemInfo.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <string_view>
#include <system_error>
#include <utility>

#include "ForkAndRun.hpp"
#include "ProgressBar.hpp"
#include "api/TgBotApi.hpp"

namespace {
std::string findVendor() {
    for (const auto& it : std::filesystem::directory_iterator("vendor/")) {
        DLOG(INFO) << "Checking: " << it;
        if (it.is_directory() &&
            std::filesystem::exists(it.path() / "config" / "common.mk")) {
            LOG(INFO) << "Found: " << it;
            return it.path().filename().string();
        }
    }
    LOG(ERROR) << "Couldn't find vendor";
    return {};
}

std::string findREL(const std::filesystem::path& releaseDir) {
    std::error_code ec;
    LOG(INFO) << "Trying to find scls in " << releaseDir;
    // Try the build/release/build_config, scl for Android 14
    for (const auto& it : std::filesystem::directory_iterator(
             releaseDir / "build_config/", ec)) {
        if (it.path().extension() == ".scl") {
            auto file = it.path().filename();
            LOG(INFO) << "Found a valid scl, " << it
                      << " release_name=" << file.replace_extension();
            return file;
        }
    }
    LOG(INFO) << "Didn't find any scl, trying textproto...";
    // Try build/release/release_configs, textproto, another stuff added on
    // Android 15
    for (const auto& it : std::filesystem::directory_iterator(
             releaseDir / "release_configs/", ec)) {
        if (it.path().extension() == ".textproto") {
            auto file = it.path().filename().replace_extension().string();
            if (file.starts_with("trunk") || file == "root") {
                continue;
            }
            LOG(INFO) << "Found a valid textproto, " << it
                      << " release_name=" << file;
            return file;
        }
    }
    LOG(INFO) << "Not found";
    // Ignore if we failed to open, these paths are only valid in Android 14+
    return {};
}
}  // namespace

DeferredExit ROMBuildTask::runFunction() {
    std::unique_ptr<ConnectedShmem> dataShmem;

    try {
        dataShmem = std::make_unique<ConnectedShmem>(
            kShmemROMBuild, sizeof(PerBuildData::ResultData));
    } catch (const syscall_perror& ex) {
        LOG(ERROR) << ex.what();
        return DeferredExit::generic_fail;
    }
    auto* resultdata = dataShmem->get<PerBuildData::ResultData>();
    resultdata->value = PerBuildData::Result::ERROR_FATAL;
    auto vendor = findVendor();
    if (vendor.empty()) {
        return DeferredExit::generic_fail;
    }
    std::string release;
    {
        std::filesystem::path path;
        path = "vendor";
        path /= vendor;
        path /= "build";
        path /= "release";
        release = findREL(path);
        if (release.empty()) {
            path = "build";
            path /= "release";
            release = findREL(path);
        }
    }

    ForkAndRunShell shell;
    // This is the build user/host config
    shell.env["BUILD_HOSTNAME"] = "build-server";
    shell.env["BUILD_USERNAME"] = "cpp20-tgbot-builder";
    if (!shell.open()) {
        return DeferredExit::generic_fail;
    }

    // Clean artifacts if exists on out directory.
    std::error_code ec;
    auto artifactDir = std::filesystem::path() / "out" / "target" / "product" /
                       data.device->codename;
    for (const auto& it :
         std::filesystem::directory_iterator(artifactDir, ec)) {
        std::error_code inner_ec;
        if (data.localManifest->rom->romInfo->artifact->match(
                it.path().filename().string())) {
            std::filesystem::remove(it.path(), inner_ec);
            if (inner_ec) {
                LOG(ERROR) << fmt::format("Error removing {}: {}",
                                          it.path().string(),
                                          inner_ec.message());
            } else {
                LOG(INFO) << "Removed: " << it.path().string();
            }
        }
    }
    if (ec &&
        ec != std::make_error_code(std::errc::no_such_file_or_directory)) {
        LOG(WARNING) << "Cannot open out directory: " << artifactDir;
    }

    std::string_view kBuildVariant;
    switch (data.variant) {
        case PerBuildData::Variant::kUser:
            kBuildVariant = "user";
            break;
        case PerBuildData::Variant::kUserDebug:
            kBuildVariant = "userdebug";
            break;
        case PerBuildData::Variant::kEng:
            kBuildVariant = "eng";
            break;
    }

    shell << "set -e" << ForkAndRunShell::endl;
    shell << ". build/envsetup.sh" << ForkAndRunShell::endl;
    shell << "unset USE_CCACHE" << ForkAndRunShell::endl;
    const auto lunch = [&, this](std::string_view release) {
        if (release.empty()) {
            return fmt::format("lunch {}_{}-{}", vendor, data.device->codename,
                               kBuildVariant);
        } else {
            return fmt::format("lunch {}_{}-{}-{}", vendor,
                               data.device->codename, release, kBuildVariant);
        }
    };
    shell << lunch(release);
    if (!release.empty()) {
        shell << ForkAndRunShell::suppress_output << ForkAndRunShell::orl
              << lunch({});
    }
    shell << ForkAndRunShell::endl;
    shell << "m " << data.localManifest->rom->romInfo->target << " -j"
          << data.localManifest->job_count << ForkAndRunShell::endl;
    auto result = shell.close();

    if (result) {
        LOG(INFO) << "ROM build succeeded";
        resultdata->value = PerBuildData::Result::SUCCESS;
    } else {
        LOG(ERROR) << "ROM build failed";
        std::ifstream errorLog(kErrorLogFile.data());
        if (errorLog.is_open()) {
            static std::regex ansi_escape_code_pattern(
                R"(\x1B\[[0-9;]*[A-Za-z])");

            std::string errorLogContent(
                (std::istreambuf_iterator<char>(errorLog)),
                std::istreambuf_iterator<char>());
            std::regex_replace(errorLogContent, ansi_escape_code_pattern, "");
            const auto commandIdx = errorLogContent.find("Command:");
            if (commandIdx != std::string::npos) {
                // Ninja error
                resultdata->setMessage(errorLogContent.substr(0, commandIdx));
            } else {
                // Probably makefile?
                resultdata->setMessage(errorLogContent);
            }
        } else {
            resultdata->setMessage("(Failed to open error log file)");
        }
    }
    return result;
}

void ROMBuildTask::handleStdoutData(ForkAndRun::BufferViewType buffer) {
    std::string buildInfoBuffer;
    const auto now = std::chrono::system_clock::now();
    const auto& rom = data.localManifest->rom;
    double memUsage = NAN;
    const auto cwd = std::filesystem::current_path();

    if (!once) [[unlikely]] {
        std::fstream ofs(kPreLogFile.data(), std::ios::app | std::ios::out);
        if (ofs) {
            ofs << buffer.data();
        }
    }

    if (clock + std::chrono::minutes(1) < now) {
        clock = now;
        once = true;
        std::string_view type;
        switch (data.variant) {
            case PerBuildData::Variant::kUser:
                type = "user";
                break;
            case PerBuildData::Variant::kUserDebug:
                type = "userdebug";
                break;
            case PerBuildData::Variant::kEng:
                type = "eng";
                break;
        }
        auto roundedTime =
            std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
        buildInfoBuffer = fmt::format(
            R"(
<blockquote>▶️ <b>Start time</b>: {} [GMT]
🕐 <b>Time spent</b>: {:%H hours %M minutes %S seconds}
🔄 <b>Last updated on</b>: {} [GMT]</blockquote>
<blockquote>🎯 <b>Target ROM</b>: {}
🏷 <b>Target branch</b>: {}
📱 <b>Device</b>: {}
🧬 <b>Build variant</b>: {}
💻 <b>CPU</b>: {}
💾 <b>Memory</b>: {}
💾 <b>Disk</b>: {}
❗️ <b>Job count</b>: {}</blockquote>
<blockquote>{}</blockquote>)",
            startTime, roundedTime, now, rom->romInfo->name, rom->branch,
            data.device->toString(), type, getPercent<CPUInfo>(),
            getPercent<MemoryInfo>(), getPercent<DiskInfo>(cwd),
            data.localManifest->job_count, buffer);
        try {
            api->editMessage<TgBotApi::ParseMode::HTML>(
                message, buildInfoBuffer, builder.get());
        } catch (const TgBot::TgException& e) {
            LOG(ERROR) << "Couldn't parse markdown, with content:";
            LOG(ERROR) << buildInfoBuffer;
        } catch (const std::exception& e) {
            LOG(ERROR) << "Error while editing message: " << e.what();
        }
        textContent->messageText = std::move(buildInfoBuffer);
    }
}

void ROMBuildTask::handleStderrData(ForkAndRun::BufferViewType buffer) {
    if (!once) [[unlikely]] {
        std::fstream ofs(kPreLogFile.data(), std::ios::app | std::ios::out);
        if (ofs) {
            ofs << buffer.data();
        }
    } else {
        std::cerr << buffer;
    }
}

void ROMBuildTask::onExit(int exitCode) {
    LOG(INFO) << "Process exited with code: " << exitCode;
    std::memcpy(data.result, smem->memory, sizeof(PerBuildData::ResultData));
}

ROMBuildTask::ROMBuildTask(TgBotApi::Ptr api, TgBot::Message::Ptr message,
                           PerBuildData data)
    : api(api),
      data(std::move(data)),
      message(std::move(message)),
      clock(std::chrono::system_clock::now()),
      startTime(clock),
      smem(std::make_unique<AllocatedShmem>(kShmemROMBuild,
                                            sizeof(PerBuildData::ResultData))) {
    auto romBuildArticle(std::make_shared<TgBot::InlineQueryResultArticle>());
    romBuildArticle->title = "Build progress";
    romBuildArticle->description = fmt::format(
        "Show the build progress running in chat {}", this->message->chat);
    romBuildArticle->id = fmt::format("rombuild-{}", this->message->messageId);
    romBuildArticle->inputMessageContent = textContent =
        std::make_shared<TgBot::InputTextMessageContent>();
    textContent->parseMode =
        TgBotApi::parseModeToStr<TgBotApi::ParseMode::HTML>();
    textContent->messageText = "Not yet ready...";
    api->addInlineQueryKeyboard(
        TgBotApi::InlineQuery{"rombuild status", "See the ROM build progress",
                              "rombuild", false, true},
        romBuildArticle);
    builder.addKeyboard({"Cancel", "cancel"});
}

ROMBuildTask::~ROMBuildTask() {
    api->removeInlineQueryKeyboard("rombuild status");
}