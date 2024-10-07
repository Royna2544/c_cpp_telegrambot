#include "UploadFileTask.hpp"

#include "ConfigParsers.hpp"
#include "ForkAndRun.hpp"

bool UploadFileTask::runFunction() {
    std::unique_ptr<ConnectedShmem> dataShmem;

    try {
        dataShmem = std::make_unique<ConnectedShmem>(
            kShmemUpload, sizeof(PerBuildData::ResultData));
    } catch (const syscall_perror& ex) {
        LOG(ERROR) << "Failed to connect to shared memory: " << ex.what();
        return false;
    }
    auto* resultdata = dataShmem->get<PerBuildData::ResultData>();

    ForkAndRunShell shell("bash");
    if (!shell.open()) {
        return false;
    }
    shell << "set -e" << ForkAndRunShell::endl;
    shell << "echo cum" << data.device
          << getValue(data.localManifest->rom)->romInfo->prefixOfOutput
          << ForkAndRunShell::endl;
    shell.close();
    return true;
}

void UploadFileTask::onExit(int exitCode) {
    switch (exitCode) {
        case EXIT_SUCCESS:
            data.result->value = PerBuildData::Result::SUCCESS;
            break;
        case EXIT_FAILURE:
            data.result->value = PerBuildData::Result::ERROR_FATAL;
            break;
        default:
            break;
    }
    LOG(INFO) << "Process exited with code: " << exitCode;
    std::memcpy(data.result, smem->memory, sizeof(PerBuildData::ResultData));
}

UploadFileTask::UploadFileTask(PerBuildData data) : data(std::move(data)) {
    smem = std::make_unique<AllocatedShmem>(kShmemUpload,
                                            sizeof(PerBuildData::ResultData));
}

UploadFileTask::~UploadFileTask() = default;
