#include "RomBuildOrchestrator.hpp"

#include <absl/log/log.h>

namespace tgbot::builder::android {

bool RomBuildOrchestrator::applySettings(const Settings& settings) {
    if (!service_->setSettings(settings)) {
        LOG(ERROR) << "Failed to apply build settings on remote";
        return false;
    }
    return true;
}

std::optional<std::string> RomBuildOrchestrator::start(
    const BuildRequest& request, IBuildObserver& observer) {
    BuildSubmission submission;
    if (!service_->startBuild(request, &submission)) {
        observer.onFailed("Failed to start build");
        return std::nullopt;
    }
    if (!submission.accepted()) {
        observer.onFailed(submission.status_message());
        return std::nullopt;
    }
    LOG(INFO) << "Started build with ID: " << submission.build_id();
    state_->start(submission.build_id());
    return submission.build_id();
}

void RomBuildOrchestrator::streamUntilDone(const std::string& buildId,
                                           IBuildObserver& observer) {
    BuildAction logRequest;
    logRequest.set_build_id(buildId);

    // Returns true if the build is still running on the server and the loop
    // should reconnect; false if it has stopped and we should exit.
    auto stillRunning = [&]() {
        BuildSubmission submission;
        if (!service_->getStatus(logRequest, &submission)) {
            LOG(ERROR) << "Failed to get build status";
            return false;
        }
        return submission.accepted();
    };

    while (state_->running()) {
        auto stream = service_->streamLogs(logRequest);
        if (!stream) {
            LOG(ERROR) << "Failed to stream logs";
            if (!stillRunning()) {
                break;
            }
            continue;  // Build still alive; retry the log stream.
        }

        stream->readAll([&](const BuildLogEntry& entry) {
            if (!state_->running()) {
                return;
            }
            ProgressEvent event;
            event.phase = BuildPhase::Building;
            event.message = entry.message();
            event.timestamp = entry.timestamp();
            observer.onProgress(event);
        });

        // The stream ended; decide whether the build is genuinely done.
        if (!stillRunning()) {
            break;
        }
    }
}

RomBuildResult RomBuildOrchestrator::collectResult(const std::string& buildId,
                                                   IBuildObserver& observer) {
    RomBuildResult result;

    if (!state_->running()) {
        result.cancelled = true;
        state_->finish();
        return result;
    }

    BuildAction request;
    request.set_build_id(buildId);

    auto stream = service_->getBuildResult(request);
    BuildResult first;
    if (!stream || !stream->readOnce(&first)) {
        result.errorMessage = "Failed to get build result stream or read from it";
        LOG(ERROR) << result.errorMessage;
        state_->finish();
        return result;
    }

    result.uploadMethod = first.upload_method();
    if (first.has_file_name()) {
        result.fileName = first.file_name();
    }

    // Failure path.
    if (!first.success()) {
        auto status = stream->finish();
        if (!first.error_message().empty()) {
            result.errorMessage = first.error_message();
        } else if (!status.ok()) {
            result.errorMessage = status.error_message();
        } else {
            result.errorMessage = "Build failed with unknown error";
        }
        LOG(ERROR) << "Build failed: " << result.errorMessage;
        state_->finish();
        return result;
    }

    // Success path.
    result.success = true;
    switch (first.upload_method()) {
        case UploadMethod::LocalFile:
            result.localFilePath = first.local_file_path();
            break;
        case UploadMethod::GoFile:
            result.gofileLink = first.gofile_link();
            break;
        case UploadMethod::Stream: {
            observer.onArtifactMeta(result.fileName, 0);
            observer.onArtifactChunk(first.stream_data().data(),
                                     first.stream_data().size());
            stream->readAll([&](const BuildResult& chunk) {
                observer.onArtifactChunk(chunk.stream_data().data(),
                                         chunk.stream_data().size());
            });
            break;
        }
        case UploadMethod::None:
        default:
            break;
    }

    auto status = stream->finish();
    if (!status.ok()) {
        result.success = false;
        result.errorMessage =
            "Build succeeded but result stream finished with error: " +
            status.error_message();
        LOG(ERROR) << result.errorMessage;
    }
    state_->finish();
    return result;
}

bool RomBuildOrchestrator::cancel(const std::string& buildId) {
    BuildAction action;
    action.set_build_id(buildId);
    return service_->cancelBuild(action);
}

}  // namespace tgbot::builder::android
