#include "ROMBuildTask.hpp"

#include <ResourceManager.h>
#include <fmt/chrono.h>
#include <fmt/core.h>

#include <SystemInfo.hpp>
#include <TgBotWrapper.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <regex>
#include <string_view>
#include <system_error>

#include "ConfigParsers.hpp"
#include "ForkAndRun.hpp"
#include "tgbot/TgException.h"

namespace {
std::string findVendor() {
    std::filesystem::directory_iterator it;
    for (it = decltype(it)("vendor/"); it != decltype(it)(); ++it) {
        DLOG(INFO) << "Checking: " << *it;
        if (it->is_directory() &&
            std::filesystem::exists(it->path() / "config" / "common.mk")) {
            LOG(INFO) << "Found: " << *it;
            return it->path().filename().string();
        }
    }
    LOG(ERROR) << "Couldn't find vendor";
    return {};
}

std::string findTCL() {
    std::error_code ec;
    std::filesystem::directory_iterator it;
    for (it = decltype(it)("build/release/build_config/", ec);
         it != decltype(it)(); ++it) {
        if (it->is_regular_file() && it->path().extension() == ".scl") {
            LOG(INFO) << "Found a valid scl, " << *it << " release_name="
                      << it->path().filename().replace_extension();
            return it->path().filename().replace_extension();
        }
    }
    LOG(INFO) << "Didn't find any scl, but it is fine";
    // Ignore if we failed to open, this path is only valid in Android 14+
    return {};
}

std::string craftPercentage(double percent) {
    constexpr std::string_view filled = "■";
    constexpr std::string_view empty = "□";
    constexpr int divider = 5;
    constexpr int totalBars = Percent::MAX / divider;
    constexpr int middleidx = totalBars / 2;

    const int greenBars = static_cast<int>(percent) / divider;
    const std::string percentStr = fmt::format(" {:.2f}% ", percent);
    const int textsize = static_cast<int>(percentStr.size());

    std::ostringstream colorBar;
    int index = 0;
    colorBar << "[";

    // Fill the bar before the percentage string
    for (; index < greenBars; ++index) {
        if (index == middleidx - textsize / 2) {
            colorBar << percentStr;
            index += textsize;
        }
        colorBar << filled;
    }

    // Fill the bar after the percentage string
    for (; index < totalBars; ++index) {
        if (index == middleidx - textsize / 2) {
            colorBar << percentStr;
            index += textsize;
        }
        colorBar << empty;
    }

    colorBar << "]";

    return colorBar.str();
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
    auto release = findTCL();
    auto vendor = findVendor();
    if (vendor.empty()) {
        return DeferredExit::generic_fail;
    }

    ForkAndRunShell shell("bash");
    shell.addEnv(
        {{"BUILD_HOSTNAME", "VM"}, {"BUILD_USERNAME", "c_cpp_telegrambot"}});
    if (!shell.open()) {
        return DeferredExit::generic_fail;
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
            return fmt::format("lunch {}_{}-{}", vendor, data.device,
                               kBuildVariant);
        } else {
            return fmt::format("lunch {}_{}-{}-{}", vendor, data.device,
                               release, kBuildVariant);
        }
    };
    shell << lunch(release);
    if (!release.empty()) {
        shell << " || " << lunch({});
    }
    shell << ForkAndRunShell::endl;
    shell << "m " << getValue(data.localManifest->rom)->romInfo->target << " -j"
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

void ROMBuildTask::onNewStdoutBuffer(ForkAndRun::BufferType& buffer) {
    std::string buildInfoBuffer;
    const auto now = std::chrono::system_clock::now();
    const auto& rom = getValue(data.localManifest->rom);
    double memUsage = NAN;
    static bool once = false;

    if (!once) {
        std::fstream ofs(kErrorLogFile.data(), std::ios::app | std::ios::out);
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
<blockquote>▶️ <b>Start time</b>: {}
🕐 <b>Time spent</b>: {:%H hours %M minutes %S seconds}
🔄 <b>Last updated on</b>: {}</blockquote>
<blockquote>🎯 <b>Target ROM</b>: {}
🏷 <b>Target branch</b>: {}
📱 <b>Device</b>: {}
🧬 <b>Build variant</b>: {}
💻 <b>CPU</b>: {}
💾 <b>Memory</b>: {}
❗️ <b>Job count</b>: {}</blockquote>
<blockquote>{}</blockquote>)",
            startTime, roundedTime, now, rom->romInfo->name, rom->branch,
            data.device, type, craftPercentage(MemoryInfo().usage().value),
            craftPercentage(CPUInfo().usage.value),
            data.localManifest->job_count, buffer.data());
        try {
            botWrapper->editMessage<TgBotWrapper::ParseMode::HTML>(
                message, buildInfoBuffer);
        } catch (const TgBot::TgException& e) {
            LOG(ERROR) << "Couldn't parse markdown, with content:";
            LOG(ERROR) << buildInfoBuffer;
        }
    }
}

void ROMBuildTask::onExit(int exitCode) {
    LOG(INFO) << "Process exited with code: " << exitCode;
    std::memcpy(data.result, smem->memory, sizeof(PerBuildData::ResultData));
}

ROMBuildTask::ROMBuildTask(ApiPtr wrapper, TgBot::Message::Ptr message,
                           PerBuildData data)
    : botWrapper(wrapper), data(std::move(data)), message(std::move(message)) {
    clock = std::chrono::system_clock::now();
    startTime = std::chrono::system_clock::now();

    // Allocate shared memory for the data object.
    smem = std::make_unique<AllocatedShmem>(kShmemROMBuild,
                                            sizeof(PerBuildData::ResultData));
}

ROMBuildTask::~ROMBuildTask() = default;