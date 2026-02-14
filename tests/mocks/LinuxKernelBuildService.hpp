#pragma once

#include <gmock/gmock.h>

#include "../../src/api/builtin_modules/builder/kernel/ILinuxKernelBuildService.hpp"

namespace tgbot::builder::linuxkernel {

/**
 * @brief Mock implementation of the Linux kernel build service interface.
 *
 * This class provides a Google Mock-based implementation of the
 * ILinuxKernelBuildService interface for use in unit tests.
 */
class MockLinuxKernelBuildService : public ILinuxKernelBuildService {
   public:
    MOCK_METHOD(bool, updateConfig,
                (const Config& request, ConfigResponse* response), (override));

    MOCK_METHOD(bool, prepareBuild,
                (const BuildPrepareRequest& request,
                 std::function<void(const BuildStatus&)> callback),
                (override));

    MOCK_METHOD(bool, doBuild,
                (const BuildRequest& request,
                 std::function<void(const BuildStatus&)> callback),
                (override));

    MOCK_METHOD(bool, cancelBuild,
                (const BuildRequest& request, BuildStatus* response),
                (override));

    MOCK_METHOD(bool, getArtifact,
                (const BuildRequest& request,
                 std::function<void(const ArtifactChunk&)> callback),
                (override));
};

}  // namespace tgbot::builder::linuxkernel
