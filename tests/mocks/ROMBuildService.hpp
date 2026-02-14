#pragma once

#include <gmock/gmock.h>

#include "../../src/api/builtin_modules/builder/android/IROMBuildService.hpp"

namespace tgbot::builder::android {

/**
 * @brief Mock implementation of the ROM build service interface.
 *
 * This class provides a Google Mock-based implementation of the
 * IROMBuildService interface for use in unit tests.
 */
class MockROMBuildService : public IROMBuildService {
   public:
    MOCK_METHOD(bool, getSettings, (Settings * response), (override));

    MOCK_METHOD(bool, setSettings, (const Settings& settings), (override));

    MOCK_METHOD(bool, cleanDirectory, (const CleanDirectoryRequest& request),
                (override));

    MOCK_METHOD(bool, directoryExists,
                (const CleanDirectoryRequest& request,
                 DirectoryExistsResponse* response),
                (override));

    MOCK_METHOD(bool, startBuild,
                (const BuildRequest& request, BuildSubmission* response),
                (override));

    MOCK_METHOD(bool, streamLogs,
                (const BuildAction& request,
                 std::function<void(const BuildLogEntry&)> callback),
                (override));

    MOCK_METHOD(bool, cancelBuild, (const BuildAction& request), (override));

    MOCK_METHOD(bool, getStatus,
                (const BuildAction& request, BuildSubmission* response),
                (override));

    MOCK_METHOD(bool, getBuildResult,
                (const BuildAction& request,
                 std::function<void(const BuildResult&)> callback),
                (override));
};

}  // namespace tgbot::builder::android
