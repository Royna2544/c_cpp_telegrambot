#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "BuildObserver.hpp"
#include "ILinuxKernelBuildService.hpp"

namespace tgbot::builder::linuxkernel {

/**
 * @brief Telegram-free orchestration of the kernel build flow.
 *
 * Owns an @ref ILinuxKernelBuildService and drives the prepare -> build ->
 * artifact pipeline, reporting progress through an @ref IBuildObserver and
 * polling @c IBuildObserver::cancelled() for cooperative abort. It has no
 * dependency on TgBotApi (or any UI), so it can be reused from a CLI, a test,
 * or any other front-end.
 */
class KernelBuildOrchestrator {
   public:
    explicit KernelBuildOrchestrator(
        std::shared_ptr<ILinuxKernelBuildService> service)
        : service_(std::move(service)) {}

    /// Push an updated configuration to the build service.
    bool updateConfig(const std::string& name, const std::string& jsonContent);

    /**
     * @brief Run "make defconfig" and stream status to @p observer.
     * @return The prepared build id on success, std::nullopt on failure.
     */
    std::optional<int> prepare(const BuildPrepareRequest& request,
                               IBuildObserver& observer);

    /**
     * @brief Execute the build, streaming status to @p observer.
     * @return true if the build completed with ProgressStatus::SUCCESS.
     */
    bool runBuild(int buildId, IBuildObserver& observer);

    /**
     * @brief Download the artifact for @p buildId to a temp file.
     *
     * Reads the metadata chunk first (validating the filename), then streams
     * the payload, emitting onArtifactMeta/onArtifactChunk along the way.
     * @param outPath Receives the path to the written file on success.
     * @return true on success.
     */
    bool downloadArtifact(int buildId, IBuildObserver& observer,
                          std::filesystem::path* outPath);

    /// Best-effort cancellation of a running build via the cancel RPC.
    bool cancel(int buildId);

   private:
    std::shared_ptr<ILinuxKernelBuildService> service_;
};

}  // namespace tgbot::builder::linuxkernel
