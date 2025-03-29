#include "UploadFileTask.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <system_error>
#include <utility>
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
    std::filesystem::path zipFilePath;

    auto matcher = data.localManifest->rom->romInfo->artifact;
    if (matcher == nullptr) {
        LOG(ERROR) << "No artifact matcher found";
        return DeferredExit::generic_fail;
    }
    const auto dir = std::filesystem::path() / "out" / "target" / "product" /
                     data.device->codename;
    for (const auto& it : std::filesystem::directory_iterator(dir)) {
        if (it.is_regular_file()) {
            auto file = it.path();
            if (matcher->match(file.filename().string())) {
                LOG(INFO) << "zipFile=" << file.string();
                zipFilePath = std::move(file);
                break;
            }
        }
    }
    if (zipFilePath.empty()) {
        LOG(ERROR) << "Artifact file not found";

        // Iterate over and print debug info.
        for (const auto& it : std::filesystem::directory_iterator(dir)) {
            if (it.is_regular_file()) {
                (void)matcher->match(it.path().filename().string(), true);
            } else {
                DLOG(INFO) << "Not a file: " << it.path();
            }
        }
        return DeferredExit::generic_fail;
    }
    std::error_code ec;
    std::filesystem::path scriptFile;
    if (std::filesystem::exists(_scriptDirectory / "upload.bash", ec)) {
        LOG(INFO) << "Using upload.bash file";
        scriptFile = _scriptDirectory / "upload.bash";
    } else {
        // Else, use default upload script.
        LOG(INFO) << "Using default upload script";
        scriptFile = _scriptDirectory / "upload.default.bash";
    }

    artifact_info.size = std::filesystem::file_size(zipFilePath, ec);
    artifact_info.filename = zipFilePath.filename().string();

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

void UploadFileTask::handleStdoutData(ForkAndRun::BufferViewType buffer) {
    const std::lock_guard<std::mutex> _(stdout_mutex);
    outputString.append(buffer.data());
}
void UploadFileTask::handleStderrData(ForkAndRun::BufferViewType buffer) {
    const std::lock_guard<std::mutex> _(stdout_mutex);
    outputString.append(buffer.data());
}

void UploadFileTask::onExit(int exitCode) {
    LOG(INFO) << "Process exited with code: " << exitCode;
    std::memcpy(data.result, smem->memory, sizeof(PerBuildData::ResultData));
    if (exitCode == EXIT_SUCCESS) {
        data.result->value = PerBuildData::Result::SUCCESS;
        const static std::regex sHttpsUrlRegex(
            R"(https:\/\/(?:[a-zA-Z0-9-]+\.)+[a-zA-Z]{2,}(?:\/[^\s]*)?)");
        std::smatch smatch;
        std::string::const_iterator search_start(outputString.cbegin());
        std::vector<std::string> urls;
        int matches = 0;

        while (std::regex_search(search_start, outputString.cend(), smatch,
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
                stream << outputString;
                stream.close();
                data.result->setMessage(fmt::format(
                    "No URLs found in output, saved to {}", path.string()));
            } else {
                data.result->setMessage(
                    "No URLs found in output, and couldn't even save to file");
                LOG(INFO) << "Script output:\n" << outputString;
            }
        } else {
            data.result->setMessage(fmt::format(R"(FileName: {}
FileSize: {}
URL(s) found on upload script output:

{})",
                                                artifact_info.filename,
                                                artifact_info.size,
                                                fmt::join(urls, "\n")));
        }
    } else {
        data.result->value = PerBuildData::Result::ERROR_FATAL;
    }
}

UploadFileTask::UploadFileTask(PerBuildData data,
                               std::filesystem::path scriptDirectory)
    : data(std::move(data)), _scriptDirectory(std::move(scriptDirectory)) {
    smem = std::make_unique<AllocatedShmem>(kShmemUpload,
                                            sizeof(PerBuildData::ResultData));
}

UploadFileTask::~UploadFileTask() = default;
