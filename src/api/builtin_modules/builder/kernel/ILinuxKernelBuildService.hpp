#pragma once

#include <memory>

#include "GrpcStream.hpp"
#include "LinuxKernelBuild_service.pb.h"

namespace tgbot::builder::linuxkernel {

/// Caller-owned stream handle (shared infrastructure).
template <typename T>
using RepeatableSource = ::tgbot::builder::RepeatableSource<T>;

/**
 * @brief Abstract interface for Linux kernel build service operations.
 *
 * This interface provides a testable abstraction layer over the gRPC-based
 * Linux kernel build service. It allows for dependency injection and makes
 * the build handler testable by enabling mock implementations.
 *
 * Streaming RPCs return a caller-owned @ref RepeatableSource so consumers can
 * peek the first message, drain the rest, and inspect the terminal
 * @c grpc::Status (mirrors the ROM build service model).
 */
class ILinuxKernelBuildService {
   public:
    virtual ~ILinuxKernelBuildService() = default;

    /**
     * @brief Update kernel configuration.
     * @return true if the RPC succeeded.
     */
    virtual bool updateConfig(const Config& request,
                              ConfigResponse* response) = 0;

    /**
     * @brief Prepare a kernel build (make defconfig).
     * @return Stream of BuildStatus updates, or nullptr if the RPC failed to
     *         start.
     */
    virtual std::unique_ptr<RepeatableSource<BuildStatus>> prepareBuild(
        const BuildPrepareRequest& request) = 0;

    /**
     * @brief Execute the actual kernel build.
     * @return Stream of BuildStatus updates, or nullptr if the RPC failed to
     *         start.
     */
    virtual std::unique_ptr<RepeatableSource<BuildStatus>> doBuild(
        const BuildRequest& request) = 0;

    /**
     * @brief Cancel an ongoing build.
     * @return true if the cancellation RPC succeeded.
     */
    virtual bool cancelBuild(const BuildRequest& request,
                             BuildStatus* response) = 0;

    /**
     * @brief Get the build artifact.
     * @return Stream of ArtifactChunk messages, or nullptr if the RPC failed to
     *         start.
     */
    virtual std::unique_ptr<RepeatableSource<ArtifactChunk>> getArtifact(
        const BuildRequest& request) = 0;
};

}  // namespace tgbot::builder::linuxkernel
