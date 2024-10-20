#include "UploadFileTask.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <libos/libfs.hpp>
#include <regex>
#include <system_error>
#include <vector>

#include "ConfigParsers.hpp"

DeferredExit UploadFileTask::runFunction() {
    std::unique_ptr<ConnectedShmem> dataShmem;

    try {
        dataShmem = std::make_unique<ConnectedShmem>(
            kShmemUpload, sizeof(PerBuildData::ResultData));
    } catch (const syscall_perror& ex) {
        LOG(ERROR) << "Failed to connect to shared memory: " << ex.what();
        return DeferredExit::generic_fail;
    }
    auto* resultdata = dataShmem->get<PerBuildData::ResultData>();

    // First determine zip file path
    std::filesystem::directory_iterator it;
    std::filesystem::path zipFilePath;

    auto matcher = getValue(data.localManifest->rom)->romInfo->artifact;
    if (matcher == nullptr) {
        LOG(ERROR) << "No artifact matcher found";
        return DeferredExit::generic_fail;
    }
    const auto dir =
        std::filesystem::path() / "out" / "target" / "product" / data.device->codename;
    for (it = decltype(it)(dir); it != decltype(it)(); ++it) {
        if (it->is_regular_file()) {
            if ((*matcher)(it->path().filename().string())) {
                LOG(INFO) << "zipFile=" << it->path().string();
                zipFilePath = it->path();
                break;
            }
        }
    }
    if (zipFilePath.empty()) {
        LOG(ERROR) << "Artifact file not found";

        // Iterate over and print debug info.
        for (it = decltype(it)(dir); it != decltype(it)(); ++it) {
            if (it->is_regular_file()) {
                (*matcher)(it->path().filename().string(), true);
            } else {
                DLOG(INFO) << "Not a file: " << it->path();
            }
        }
        return DeferredExit::generic_fail;
    }
    std::error_code ec;
    const auto scripts =
        FS::getPathForType(FS::PathType::RESOURCES) / "scripts";
    std::filesystem::path scriptFile;
    if (std::filesystem::exists(scripts / "upload.bash", ec)) {
        LOG(INFO) << "Using upload.bash file";
        scriptFile = scripts / "upload.bash";
    } else {
        // Else, use default upload script.
        scriptFile = scripts / "upload.default.bash";
    }

    // Run the upload script.
    LOG(INFO) << "Starting upload";
    ForkAndRunShell shell("bash");
    if (!shell.open()) {
        return DeferredExit::generic_fail;
    }
    shell << "bash " << scriptFile << " " << zipFilePath.string()
          << ForkAndRunShell::endl;
    return shell.close();
}

void UploadFileTask::onNewStdoutBuffer(ForkAndRun::BufferType& buffer) {
    stdoutOutput.append(buffer.data());
}

void UploadFileTask::onExit(int exitCode) {
    LOG(INFO) << "Process exited with code: " << exitCode;
    std::memcpy(data.result, smem->memory, sizeof(PerBuildData::ResultData));
    if (exitCode == EXIT_SUCCESS) {
        data.result->value = PerBuildData::Result::SUCCESS;
        const static std::regex sHttpsUrlRegex(
            R"(https:\/\/(?:[a-zA-Z0-9-]+\.)+[a-zA-Z]{2,}(?:\/[^\s]*)?)");
        std::smatch smatch;
        std::string::const_iterator search_start(stdoutOutput.cbegin());
        std::vector<std::string> urls;
        int matches = 0;

        while (std::regex_search(search_start, stdoutOutput.cend(), smatch,
                                 sHttpsUrlRegex)) {
            LOG(INFO) << "Found URL: " << smatch[0].str();
            urls.emplace_back(
                fmt::format("[{}] {}", ++matches, smatch[0].str()));
            // Move the iterator to the end of the current match to avoid
            // infinite loop
            search_start = smatch.suffix().first;
        }
        if (urls.empty()) {
            LOG(WARNING) << "No URLs found in output";
            std::filesystem::path path(
                fmt::format("upload_output_{}.txt", getpid()));
            std::ofstream stream(path);
            if (stream) {
                stream << stdoutOutput;
                stream.close();
                data.result->setMessage(fmt::format(
                    "No URLs found in output, saved to {}", path.string()));
            } else {
                data.result->setMessage(
                    "No URLs found in output, and couldn't even save to file");
                LOG(INFO) << "Script output:\n" << stdoutOutput;
            }
        } else {
            data.result->setMessage(
                fmt::format("URLs grabbed from upload script output:\n\n{}",
                            fmt::join(urls, "\n")));
        }
    } else {
        data.result->value = PerBuildData::Result::ERROR_FATAL;
    }
}

UploadFileTask::UploadFileTask(PerBuildData data) : data(std::move(data)) {
    smem = std::make_unique<AllocatedShmem>(kShmemUpload,
                                            sizeof(PerBuildData::ResultData));
}

UploadFileTask::~UploadFileTask() = default;
