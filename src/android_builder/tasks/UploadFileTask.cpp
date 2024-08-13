#include "UploadFileTask.hpp"

#include <ArgumentBuilder.hpp>

#include "ConfigParsers.hpp"

bool UploadFileTask::runFunction() {
    const auto py = PythonClass::get();
    auto dataShmem =
        connectShmem(kShmemUpload, sizeof(PerBuildData::ResultData));
    if (!dataShmem) {
        LOG(ERROR) << "Could not allocate shared memory";
        return false;
    }
    auto* resultdata =
        static_cast<PerBuildData::ResultData*>(dataShmem->memory);
    const auto mod = py->importModule("upload_file");
    if (!mod) {
        LOG(ERROR) << "Could not import module upload_file";
        resultdata->setMessage("Could not import module upload_file");
        disconnectShmem(dataShmem.value());
        return false;
    }
    // Device name, PrefixStr
    auto func = mod->lookupFunction("upload_to_gofile");
    if (!func) {
        LOG(ERROR) << "Could not find function upload_to_gofile";
        resultdata->setMessage("Could not find function upload_to_gofile");
        disconnectShmem(dataShmem.value());
        return false;
    }
    ArgumentBuilder builder(2);
    builder.add_argument(data.device);
    builder.add_argument(getValue(data.localManifest->rom)
            ->romInfo->prefixOfOutput);
    std::string resultString;
    if (!func->call(builder.build(), &resultString)) {
        LOG(ERROR) << "Error calling function upload_to_gofile";
        resultdata->setMessage("Error calling function upload_to_gofile");
        disconnectShmem(dataShmem.value());
        return false;
    }
    LOG(INFO) << resultString;
    data.result->setMessage(resultString);
    disconnectShmem(dataShmem.value());
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
    std::memcpy(data.result, smem.memory, sizeof(PerBuildData::ResultData));
}

UploadFileTask::UploadFileTask(PerBuildData data) : data(std::move(data)) {
    auto shmem = allocShmem(kShmemUpload, sizeof(PerBuildData::ResultData));
    if (!shmem) {
        LOG(ERROR) << "Could not allocate shared memory";
        throw std::runtime_error("Could not allocate shared memory");
    }
    smem = shmem.value();
}

UploadFileTask::~UploadFileTask() { freeShmem(smem); }
