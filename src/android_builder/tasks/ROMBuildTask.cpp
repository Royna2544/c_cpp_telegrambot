#include "ROMBuildTask.hpp"

#include <BotReplyMessage.h>

#include <ArgumentBuilder.hpp>
#include <mutex>

bool ROMBuildTask::runFunction() {
    auto dataShmem =
        connectShmem(kShmemROMBuild, sizeof(PerBuildData::ResultData));
    if (!dataShmem) {
        LOG(ERROR) << "Failed to connect to shared memory";
        return false;
    }
    auto* resultdata =
        static_cast<PerBuildData::ResultData*>(dataShmem->memory);
    resultdata->value = PerBuildData::Result::ERROR_FATAL;
    auto repomod = _py->importModule("build_rom_utils");
    if (!repomod) {
        resultdata->setMessage("Failed to import build_rom_utils module");
        disconnectShmem(dataShmem.value());
        return false;
    }
    auto build_rom = repomod->lookupFunction("build_rom");
    if (!build_rom) {
        resultdata->setMessage("Failed to find build_rom function");
        disconnectShmem(dataShmem.value());
        return false;
    }
    ArgumentBuilder builder(4);
    builder.add_argument(data.bConfig.device);
    switch (data.bConfig.variant) {
        case BuildConfig::Variant::kUser:
            builder.add_argument("user");
            break;
        case BuildConfig::Variant::kUserDebug:
            builder.add_argument("userdebug");
            break;
        case BuildConfig::Variant::kEng:
            builder.add_argument("eng");
            break;
    }
    builder.add_argument(data.rConfig.target);
    builder.add_argument(guessJobCount());
    auto* arg = builder.build();
    if (arg == nullptr) {
        resultdata->setMessage("Failed to build arguments");
        disconnectShmem(dataShmem.value());
        return false;
    }
    bool result = false;
    if (!build_rom->call<bool>(arg, &result)) {
        resultdata->setMessage("Failed to call build ROM");
        disconnectShmem(dataShmem.value());
        return false;
    }
    Py_DECREF(arg);
    if (result) {
        LOG(INFO) << "ROM build succeeded";
        resultdata->value = PerBuildData::Result::SUCCESS;
    } else {
        LOG(ERROR) << "ROM build failed";
        std::ifstream errorLog(kErrorLogFile.data());
        if (errorLog.is_open()) {
            std::string errorLogContent(
                (std::istreambuf_iterator<char>(errorLog)),
                std::istreambuf_iterator<char>());
            if (errorLogContent.find("FAILED:") != std::string::npos) {
                // Ninja error
                resultdata->setMessage(errorLogContent.substr(
                    errorLogContent.find_first_of('\n')));
            } else {
                // Probably makefile?
                resultdata->setMessage(errorLogContent);
            }
        } else {
            resultdata->setMessage("(Failed to open error log file)");
        }
    }
    disconnectShmem(dataShmem.value());
    return result;
}

int ROMBuildTask::guessJobCount() {
    static constexpr int Multiplier = 1024;
    static constexpr int kDefaultJobCount = 6;
    static std::once_flag once;
    static int jobCount = kDefaultJobCount;
    std::call_once(once, [this] {
        double total_memory = NAN;
        if (!_get_total_mem->call<double>(nullptr, &total_memory)) {
            return;
        }
        total_memory /= Multiplier;  // Convert to GB
        LOG(INFO) << "Total memory: " << total_memory << "GB";
        jobCount = static_cast<int>(sqrt(total_memory) * 2);
        LOG(INFO) << "Using job count: " << jobCount;
    });
    return jobCount;
}

void ROMBuildTask::onNewStdoutBuffer(ForkAndRun::BufferType& buffer) {
    std::stringstream buildInfoBuffer;
    const auto now = std::chrono::system_clock::now();
    double memUsage = NAN;
    if (clock + std::chrono::minutes(1) < now) {
        clock = now;
        buildInfoBuffer << "Start time: " << fromTP(startTime) << std::endl;
        buildInfoBuffer << "Time spent: " << to_string(now - startTime)
                        << std::endl;
        buildInfoBuffer << "Last updated on: " << fromTP(now) << std::endl;
        buildInfoBuffer << "Target ROM: " << data.rConfig.name
                        << " branch: " << data.rConfig.branch;
        buildInfoBuffer << "Target device: " << data.bConfig.device << std::endl;
        buildInfoBuffer << "Job count: " << guessJobCount();
        if (_get_used_mem->call(nullptr, &memUsage)) {
            buildInfoBuffer << ", memory usage: " << memUsage << "%";
        } else {
            buildInfoBuffer << ", memory usage: unavailable";
        }
        buildInfoBuffer << std::endl;
        buildInfoBuffer << "Variant: ";
        switch (data.bConfig.variant) {
            case BuildConfig::Variant::kUser:
                buildInfoBuffer << "user";
                break;
            case BuildConfig::Variant::kUserDebug:
                buildInfoBuffer << "userdebug";
                break;
            case BuildConfig::Variant::kEng:
                buildInfoBuffer << "eng";
                break;
        }
        buildInfoBuffer << std::endl << std::endl;
        bot_editMessage(_bot, message, buildInfoBuffer.str() + buffer.data());
    }
}

void ROMBuildTask::onExit(int exitCode) {
    // switch (exitCode) { case EXIT_SUCCESS: };
    LOG(INFO) << "Process exited with code: " << exitCode;
    std::memcpy(data.result, smem.memory, sizeof(PerBuildData::ResultData));
}

[[noreturn]] void ROMBuildTask::errorAndThrow(const std::string& message) {
    LOG(ERROR) << message;
    throw std::runtime_error(message);
}

ROMBuildTask::ROMBuildTask(const TgBot::Bot& bot, TgBot::Message::Ptr message,
                           PerBuildData data)
    : BotClassBase(bot), data(std::move(data)), message(std::move(message)) {
    clock = std::chrono::system_clock::now();
    startTime = std::chrono::system_clock::now();

    _py = PythonClass::get();
    _py->addLookupDirectory(data.scriptDirectory);
    auto repomod = _py->importModule("system_info");

    // Lookup python functions
    if (!repomod) {
        errorAndThrow("Failed to import system_info module");
    }
    // Get total memory function
    _get_total_mem = repomod->lookupFunction("get_memory_total");
    if (!_get_total_mem) {
        errorAndThrow("Failed to find get_memory_total function");
    }
    // Get used memory function
    _get_used_mem = repomod->lookupFunction("get_memory_usage");
    if (!_get_used_mem) {
        errorAndThrow("Failed to find get_memory_usage function");
    }
    // Allocate shared memory for the data object.
    auto shmem = allocShmem(kShmemROMBuild, sizeof(PerBuildData::ResultData));
    if (!shmem) {
        errorAndThrow("Failed to allocate shared memory");
    }
    smem = shmem.value();
}
ROMBuildTask::~ROMBuildTask() { freeShmem(smem); }