#pragma once

#include <memory>
#include <optional>
#include <string>

#include "BuildObserver.hpp"
#include "BuildState.hpp"
#include "IROMBuildService.hpp"

namespace tgbot::builder::android {

/// Transport-agnostic outcome of a completed (or failed) ROM build.
struct RomBuildResult {
    bool success = false;
    bool cancelled = false;
    UploadMethod uploadMethod = UploadMethod::None;
    std::string localFilePath;  ///< Set when uploadMethod == LocalFile.
    std::string gofileLink;     ///< Set when uploadMethod == GoFile.
    std::string fileName;       ///< Suggested artifact name, if provided.
    std::string errorMessage;   ///< Populated on failure.
};

/**
 * @brief Telegram-free orchestration of the Android ROM build flow.
 *
 * Owns an @ref IROMBuildService and a shared @ref BuildState (so the UI can
 * request cancellation). It drives start -> stream logs -> collect result,
 * reporting progress and streamed artifact bytes through an
 * @ref IBuildObserver. No dependency on TgBotApi.
 */
class RomBuildOrchestrator {
   public:
    RomBuildOrchestrator(std::shared_ptr<IROMBuildService> service,
                         BuildState* state)
        : service_(std::move(service)), state_(state) {}

    /// Apply build settings on the remote (best effort, logged on failure).
    bool applySettings(const Settings& settings);

    /**
     * @brief Submit a build request.
     * @return The accepted build id, or std::nullopt (observer is notified of
     *         the rejection / RPC failure). On success the shared BuildState is
     *         marked running.
     */
    std::optional<std::string> start(const BuildRequest& request,
                                     IBuildObserver& observer);

    /**
     * @brief Stream build logs until the build stops or is cancelled.
     *
     * Reconnects to the log stream while the BuildState reports running,
     * mirroring the previous resilience behaviour. Each log entry is pushed to
     * the observer as a ProgressEvent.
     */
    void streamUntilDone(const std::string& buildId, IBuildObserver& observer);

    /**
     * @brief Fetch the final build result.
     *
     * For Stream uploads the payload bytes are emitted via
     * IBuildObserver::onArtifactMeta / onArtifactChunk; the caller decides how
     * to persist/deliver them. Marks the BuildState finished before returning.
     */
    RomBuildResult collectResult(const std::string& buildId,
                                 IBuildObserver& observer);

    /// Best-effort server-side cancellation.
    bool cancel(const std::string& buildId);

   private:
    std::shared_ptr<IROMBuildService> service_;
    BuildState* state_;
};

}  // namespace tgbot::builder::android
