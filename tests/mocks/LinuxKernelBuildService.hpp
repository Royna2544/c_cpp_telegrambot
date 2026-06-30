#pragma once

#include <gmock/gmock.h>

#include <memory>

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

    MOCK_METHOD(std::unique_ptr<RepeatableSource<BuildStatus>>, prepareBuild,
                (const BuildPrepareRequest& request), (override));

    MOCK_METHOD(std::unique_ptr<RepeatableSource<BuildStatus>>, doBuild,
                (const BuildRequest& request), (override));

    MOCK_METHOD(bool, cancelBuild,
                (const BuildRequest& request, BuildStatus* response),
                (override));

    MOCK_METHOD(std::unique_ptr<RepeatableSource<ArtifactChunk>>, getArtifact,
                (const BuildRequest& request), (override));
};

}  // namespace tgbot::builder::linuxkernel
