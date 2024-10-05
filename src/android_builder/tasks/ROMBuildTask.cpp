#include "ROMBuildTask.hpp"

#include <SystemInfo.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <regex>
#include <system_error>

#include "ConfigParsers.hpp"
#include "ForkAndRun.hpp"
#include "TgBotWrapper.hpp"

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
    // Ignore if we failed to open, this path is only valid in Android 14+
    return {};
}
}  // namespace

bool ROMBuildTask::runFunction() {
    std::unique_ptr<ConnectedShmem> dataShmem;

    try {
        dataShmem = std::make_unique<ConnectedShmem>(
            kShmemROMBuild, sizeof(PerBuildData::ResultData));
    } catch (const syscall_perror& ex) {
        LOG(ERROR) << ex.what();
        return false;
    }
    auto* resultdata = dataShmem->get<PerBuildData::ResultData>();
    resultdata->value = PerBuildData::Result::ERROR_FATAL;
    ForkAndRunShell shell("bash");
    if (!shell.open()) {
        return false;
    }

    shell << "set -e" << ForkAndRunShell::endl;
    shell << ". build/envsetup.sh" << ForkAndRunShell::endl;
    auto release = findTCL();
    if (release.empty()) {
        shell << "lunch " << findVendor() << "_" << data.device << "-";
    } else {
        shell << "lunch " << findVendor() << "_" << data.device << "-"
              << release << "-";
    }
    switch (data.variant) {
        case PerBuildData::Variant::kUser:
            shell << "user";
            break;
        case PerBuildData::Variant::kUserDebug:
            shell << "userdebug";
            break;
        case PerBuildData::Variant::kEng:
            shell << "eng";
            break;
    }
    shell << ForkAndRunShell::endl;
    shell << "m " << getValue(data.localManifest->rom)->romInfo->target << " -j"
          << guessJobCount() << ForkAndRunShell::endl;
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

int ROMBuildTask::guessJobCount() {
    static constexpr int Multiplier = 1024;
    static constexpr int kDefaultJobCount = 6;

    static std::once_flag once;
    static int jobCount = kDefaultJobCount;
    std::call_once(once, [this] {
        MemoryInfo info;
        const auto totalMem =
            MemoryInfo().totalMemory.to<SizeTypes::GigaBytes>();
        LOG(INFO) << "Total memory: " << totalMem;
        jobCount = static_cast<int>(sqrt(totalMem));
        LOG(INFO) << "Using job count: " << jobCount;
    });
    return jobCount;
}

void ROMBuildTask::onNewStdoutBuffer(ForkAndRun::BufferType& buffer) {
    std::stringstream buildInfoBuffer;
    const auto now = std::chrono::system_clock::now();
    const auto& rom = getValue(data.localManifest->rom);
    double memUsage = NAN;
    if (clock + std::chrono::minutes(1) < now) {
        clock = now;
        buildInfoBuffer << "Start time: " << fromTP(startTime) << std::endl;
        buildInfoBuffer << "Time spent: " << to_string(now - startTime)
                        << std::endl;
        buildInfoBuffer << "Last updated on: " << fromTP(now) << std::endl;
        buildInfoBuffer << "Target ROM: " << rom->romInfo->name
                        << ", branch: " << rom->branch << std::endl;
        buildInfoBuffer << "Target device: " << data.device << std::endl;
        buildInfoBuffer << "Job count: " << guessJobCount();
        buildInfoBuffer << ", memory usage: " << MemoryInfo().usage();
        buildInfoBuffer << std::endl;
        buildInfoBuffer << "Variant: ";
        switch (data.variant) {
            case PerBuildData::Variant::kUser:
                buildInfoBuffer << "user";
                break;
            case PerBuildData::Variant::kUserDebug:
                buildInfoBuffer << "userdebug";
                break;
            case PerBuildData::Variant::kEng:
                buildInfoBuffer << "eng";
                break;
        }
        buildInfoBuffer << std::endl << std::endl;
        buildInfoBuffer << buffer.data();
        botWrapper->editMessage(message, buildInfoBuffer.str());
    }
}

void ROMBuildTask::onExit(int exitCode) {
    LOG(INFO) << "Process exited with code: " << exitCode;
    std::memcpy(data.result, smem->memory, sizeof(PerBuildData::ResultData));
}

[[noreturn]] void ROMBuildTask::errorAndThrow(const std::string& message) {
    LOG(ERROR) << message;
    throw std::runtime_error(message);
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