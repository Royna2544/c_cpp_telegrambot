#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ROMBuild_service.pb.h"

namespace tgbot::builder::android {

/**
 * @brief Abstract interface for Android ROM build service operations.
 *
 * This interface provides a testable abstraction layer over the gRPC-based
 * Android ROM build service. It allows for dependency injection and makes
 * the build handler testable by enabling mock implementations.
 */
class IROMBuildService {
   public:
    virtual ~IROMBuildService() = default;

    /**
     * @brief Get current build settings.
     *
     * @param response The response containing current settings.
     * @return true if the operation succeeded, false otherwise.
     */
    virtual bool getSettings(Settings* response) = 0;

    /**
     * @brief Set build settings.
     *
     * @param settings The settings to apply.
     * @return true if the operation succeeded, false otherwise.
     */
    virtual bool setSettings(const Settings& settings) = 0;

    /**
     * @brief Clean a specified directory.
     *
     * @param request The clean directory request.
     * @return true if the operation succeeded, false otherwise.
     */
    virtual bool cleanDirectory(const CleanDirectoryRequest& request) = 0;

    /**
     * @brief Check if a directory exists.
     *
     * @param request The directory to check.
     * @param response The response containing existence status.
     * @return true if the operation succeeded, false otherwise.
     */
    virtual bool directoryExists(const CleanDirectoryRequest& request,
                                 DirectoryExistsResponse* response) = 0;

    /**
     * @brief Start a new ROM build.
     *
     * @param request The build request.
     * @param response The build submission response with build ID.
     * @return true if the operation succeeded, false otherwise.
     */
    virtual bool startBuild(const BuildRequest& request,
                            BuildSubmission* response) = 0;

    /**
     * @brief Stream build logs.
     *
     * @param request The build action (build ID).
     * @param callback Callback function invoked for each log entry.
     * @return true if the operation succeeded, false otherwise.
     */
    virtual bool streamLogs(
        const BuildAction& request,
        std::function<void(const BuildLogEntry&)> callback) = 0;

    /**
     * @brief Cancel a build in progress.
     *
     * @param request The build action (build ID).
     * @return true if the operation succeeded, false otherwise.
     */
    virtual bool cancelBuild(const BuildAction& request) = 0;

    /**
     * @brief Get the status of a build.
     *
     * @param request The build action (build ID).
     * @param response The build submission response with status.
     * @return true if the operation succeeded, false otherwise.
     */
    virtual bool getStatus(const BuildAction& request,
                           BuildSubmission* response) = 0;

    /**
     * @brief Get the build result.
     *
     * @param request The build action (build ID).
     * @param callback Callback function invoked for each result chunk.
     * @return true if the operation succeeded, false otherwise.
     */
    virtual bool getBuildResult(
        const BuildAction& request,
        std::function<void(const BuildResult&)> callback) = 0;
};

}  // namespace tgbot::builder::android
