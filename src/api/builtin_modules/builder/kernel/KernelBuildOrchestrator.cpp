#include "KernelBuildOrchestrator.hpp"

#include <absl/log/log.h>
#include <fmt/format.h>

#include <fstream>
#include <system_error>

namespace tgbot::builder::linuxkernel {

namespace {

BuildPhase mapPhase(ProgressStatus status) {
    switch (status) {
        case ProgressStatus::PENDING:
            return BuildPhase::Queued;
        case ProgressStatus::IN_PROGRESS_CONFIGURE:
            return BuildPhase::Configuring;
        case ProgressStatus::IN_PROGRESS_DOWNLOAD:
            return BuildPhase::Downloading;
        case ProgressStatus::IN_PROGRESS_BUILD:
            return BuildPhase::Building;
        case ProgressStatus::SUCCESS:
            return BuildPhase::Completed;
        case ProgressStatus::FAILED:
            return BuildPhase::Failed;
        case ProgressStatus::NONE:
        default:
            return BuildPhase::Queued;
    }
}

ProgressEvent toEvent(const BuildStatus& status, BuildPhase fallback) {
    ProgressEvent event;
    event.phase = status.status() == ProgressStatus::NONE
                      ? fallback
                      : mapPhase(status.status());
    event.message = status.output();
    return event;
}

}  // namespace

bool KernelBuildOrchestrator::updateConfig(const std::string& name,
                                           const std::string& jsonContent) {
    Config request;
    request.set_name(name);
    request.set_json_content(jsonContent);
    ConfigResponse response;
    if (!service_->updateConfig(request, &response) || !response.success()) {
        LOG(ERROR) << "Failed to update kernel config '" << name
                   << "': " << response.message();
        return false;
    }
    return true;
}

std::optional<int> KernelBuildOrchestrator::prepare(
    const BuildPrepareRequest& request, IBuildObserver& observer) {
    auto stream = service_->prepareBuild(request);
    if (!stream) {
        observer.onFailed("Failed to start prepare stream.");
        return std::nullopt;
    }

    int buildId = 0;
    BuildStatus last;
    stream->readAll([&](const BuildStatus& status) {
        last = status;
        if (status.build_id() != 0) {
            buildId = status.build_id();
        }
        if (!observer.cancelled()) {
            observer.onProgress(toEvent(status, BuildPhase::Preparing));
        }
    });

    auto finishStatus = stream->finish();
    if (!finishStatus.ok()) {
        observer.onFailed(fmt::format("Prepare failed: {}",
                                      finishStatus.error_message()));
        return std::nullopt;
    }
    if (last.status() != ProgressStatus::SUCCESS) {
        observer.onFailed(
            fmt::format("Prepare incomplete: {}", last.output()));
        return std::nullopt;
    }
    return buildId;
}

bool KernelBuildOrchestrator::runBuild(int buildId, IBuildObserver& observer) {
    BuildRequest request;
    request.set_build_id(buildId);

    auto stream = service_->doBuild(request);
    if (!stream) {
        observer.onFailed("Failed to start build stream.");
        return false;
    }

    BuildStatus last;
    stream->readAll([&](const BuildStatus& status) {
        last = status;
        if (!observer.cancelled()) {
            observer.onProgress(toEvent(status, BuildPhase::Building));
        }
    });

    auto finishStatus = stream->finish();
    if (!finishStatus.ok()) {
        observer.onFailed(
            fmt::format("Build failed: {}", finishStatus.error_message()));
        return false;
    }
    if (last.status() != ProgressStatus::SUCCESS) {
        observer.onFailed(fmt::format("Build incomplete: {}", last.output()));
        return false;
    }
    return true;
}

bool KernelBuildOrchestrator::downloadArtifact(
    int buildId, IBuildObserver& observer, std::filesystem::path* outPath) {
    BuildRequest request;
    request.set_build_id(buildId);

    auto stream = service_->getArtifact(request);
    if (!stream) {
        observer.onFailed("Failed to start artifact stream.");
        return false;
    }

    // First message must carry metadata.
    ArtifactChunk chunk;
    if (!stream->readOnce(&chunk) || !chunk.has_metadata()) {
        observer.onFailed("No artifact metadata received.");
        return false;
    }

    const auto safeFilename =
        std::filesystem::path(chunk.metadata().filename()).filename();
    if (safeFilename.empty() || safeFilename == "." || safeFilename == "..") {
        observer.onFailed("Build service returned an invalid artifact name.");
        return false;
    }

    const auto localPath =
        std::filesystem::temp_directory_path() /
        fmt::format("kernelbuild_{}_{}", buildId, safeFilename.string());
    std::ofstream outputFile(localPath, std::ios::binary);
    if (!outputFile.is_open()) {
        observer.onFailed("Failed to open output file for writing.");
        return false;
    }

    observer.onArtifactMeta(chunk.metadata().filename(),
                            chunk.metadata().total_size());

    stream->readAll([&](const ArtifactChunk& dataChunk) {
        const auto& data = dataChunk.data();
        outputFile.write(data.data(), static_cast<std::streamsize>(data.size()));
        observer.onArtifactChunk(data.data(), data.size());
    });
    outputFile.close();

    auto finishStatus = stream->finish();
    if (!finishStatus.ok()) {
        observer.onFailed(fmt::format("Failed to retrieve artifact: {}",
                                      finishStatus.error_message()));
        std::error_code ec;
        std::filesystem::remove(localPath, ec);
        return false;
    }

    *outPath = localPath;
    return true;
}

bool KernelBuildOrchestrator::cancel(int buildId) {
    BuildRequest request;
    request.set_build_id(buildId);
    BuildStatus response;
    return service_->cancelBuild(request, &response);
}

}  // namespace tgbot::builder::linuxkernel
