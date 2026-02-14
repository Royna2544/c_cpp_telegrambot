#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "LinuxKernelBuild_service.pb.h"

namespace tgbot::builder::linuxkernel {

/**
 * @brief Abstract interface for Linux kernel build service operations.
 *
 * This interface provides a testable abstraction layer over the gRPC-based
 * Linux kernel build service. It allows for dependency injection and makes
 * the build handler testable by enabling mock implementations.
 */
class ILinuxKernelBuildService {
   public:
    virtual ~ILinuxKernelBuildService() = default;

    /**
     * @brief Update kernel configuration.
     *
     * @param request The configuration update request.
     * @param response The response containing success status and message.
     * @return true if the operation succeeded, false otherwise.
     */
    virtual bool updateConfig(const Config& request,
                              ConfigResponse* response) = 0;

    /**
     * @brief Prepare a kernel build (make defconfig).
     *
     * @param request The build preparation request.
     * @param callback Callback function invoked for each status update.
     * @return true if the preparation succeeded, false otherwise.
     */
    virtual bool prepareBuild(
        const BuildPrepareRequest& request,
        std::function<void(const BuildStatus&)> callback) = 0;

    /**
     * @brief Execute the actual kernel build.
     *
     * @param request The build request containing the build ID.
     * @param callback Callback function invoked for each status update.
     * @return true if the build succeeded, false otherwise.
     */
    virtual bool doBuild(const BuildRequest& request,
                         std::function<void(const BuildStatus&)> callback) = 0;

    /**
     * @brief Cancel an ongoing build.
     *
     * @param request The build request containing the build ID to cancel.
     * @param response The response containing the cancellation status.
     * @return true if the cancellation succeeded, false otherwise.
     */
    virtual bool cancelBuild(const BuildRequest& request,
                             BuildStatus* response) = 0;

    /**
     * @brief Get the build artifact.
     *
     * @param request The build request containing the build ID.
     * @param callback Callback function invoked for each artifact chunk.
     * @return true if the artifact retrieval succeeded, false otherwise.
     */
    virtual bool getArtifact(
        const BuildRequest& request,
        std::function<void(const ArtifactChunk&)> callback) = 0;
};

}  // namespace tgbot::builder::linuxkernel
