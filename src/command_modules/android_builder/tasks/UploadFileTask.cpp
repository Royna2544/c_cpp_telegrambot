#include "UploadFileTask.hpp"

#include <filesystem>
#include <libos/libfs.hpp>
#include <system_error>

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

    // First determine zip file path
    std::filesystem::directory_iterator it;
    std::filesystem::path zipFilePath;

    for (it = decltype(it)(std::filesystem::path() / "out" / "target" /
                           "product" / data.device);
         it != decltype(it)(); ++it) {
        if (it->is_regular_file() && it->path().extension() == ".zip") {
            DLOG(INFO) << "Entry: " << it->path();
            if (it->path().filename().string().starts_with(
                    getValue(data.localManifest->rom)
                        ->romInfo->prefixOfOutput)) {
                LOG(INFO) << "zipFile=" << it->path().string();
                zipFilePath = it->path();
                break;
            }
        }
    }
    if (zipFilePath.empty()) {
        LOG(ERROR) << "Zip file not found";
        return false;
    }
    std::error_code ec;
    const auto scripts =
        FS::getPathForType(FS::PathType::RESOURCES) / "scripts";
    std::filesystem::path scriptFile;
    if (std::filesystem::exists(scripts / "upload.bash", ec)) {
        LOG(INFO) << "Using upload.bash file";
        scriptFile = scripts / "upload.bash";
    }

    // Else, use default upload script.
    scriptFile = scripts / "upload.default.bash";

    // Run the upload script.
    LOG(INFO) << "Starting upload";
    ForkAndRunShell shell("bash");
    if (!shell.open()) {
        return false;
    }
    shell << "bash " << scriptFile << " " << zipFilePath.string()
          << ForkAndRunShell::endl;
    shell.close();
    return true;
}

void UploadFileTask::onExit(int exitCode) {
    LOG(INFO) << "Process exited with code: " << exitCode;
    std::memcpy(data.result, smem->memory, sizeof(PerBuildData::ResultData));
    if (exitCode == EXIT_SUCCESS) {
        data.result->value = PerBuildData::Result::SUCCESS;
    } else {
        data.result->value = PerBuildData::Result::ERROR_FATAL;
    }
}

UploadFileTask::UploadFileTask(PerBuildData data) : data(std::move(data)) {
    smem = std::make_unique<AllocatedShmem>(kShmemUpload,
                                            sizeof(PerBuildData::ResultData));
}

UploadFileTask::~UploadFileTask() = default;
