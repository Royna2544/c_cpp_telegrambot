#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mocks/ROMBuildService.hpp"

using namespace tgbot::builder::android;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;

/**
 * @brief Test fixture for the ROM Build Service interface.
 *
 * This test demonstrates how the build service interface enables testing
 * by allowing mock implementations to be injected.
 */
class ROMBuildServiceTest : public ::testing::Test {
   protected:
    std::shared_ptr<MockROMBuildService> mockService;

    void SetUp() override {
        mockService = std::make_shared<MockROMBuildService>();
    }
};

/**
 * @brief Test that setSettings method can be mocked and used.
 */
TEST_F(ROMBuildServiceTest, SetSettingsSuccess) {
    Settings settings;
    settings.set_do_repo_sync(true);
    settings.set_do_upload(false);

    EXPECT_CALL(*mockService, setSettings(_)).WillOnce(Return(true));

    bool result = mockService->setSettings(settings);

    EXPECT_TRUE(result);
}

/**
 * @brief Test that getSettings method works.
 */
TEST_F(ROMBuildServiceTest, GetSettingsSuccess) {
    Settings expectedSettings;
    expectedSettings.set_do_repo_sync(true);
    expectedSettings.set_use_ccache(true);

    EXPECT_CALL(*mockService, getSettings(_))
        .WillOnce(
            DoAll(Invoke([&](Settings* resp) { *resp = expectedSettings; }),
                  Return(true)));

    Settings actualSettings;
    bool result = mockService->getSettings(&actualSettings);

    EXPECT_TRUE(result);
    EXPECT_TRUE(actualSettings.do_repo_sync());
    EXPECT_TRUE(actualSettings.use_ccache());
}

/**
 * @brief Test that startBuild method works.
 */
TEST_F(ROMBuildServiceTest, StartBuildSuccess) {
    BuildRequest request;
    request.set_config_name("test_rom");
    request.set_target_device("sailfish");
    request.set_rom_name("LineageOS");
    request.set_rom_android_version(13.0);
    request.set_build_variant(BuildVariant::UserDebug);

    BuildSubmission expectedResponse;
    expectedResponse.set_build_id("build-12345");
    expectedResponse.set_accepted(true);
    expectedResponse.set_status_message("Build started");

    EXPECT_CALL(*mockService, startBuild(_, _))
        .WillOnce(
            DoAll(Invoke([&](const BuildRequest& req, BuildSubmission* resp) {
                      *resp = expectedResponse;
                  }),
                  Return(true)));

    BuildSubmission actualResponse;
    bool result = mockService->startBuild(request, &actualResponse);

    EXPECT_TRUE(result);
    EXPECT_EQ(actualResponse.build_id(), "build-12345");
    EXPECT_TRUE(actualResponse.accepted());
    EXPECT_EQ(actualResponse.status_message(), "Build started");
}

template <typename T>
struct TestRepeatableSource : public IROMBuildService::RepeatableSource<T> {
    std::vector<T> entries;
    size_t currentIndex = 0;

    explicit TestRepeatableSource(const std::vector<T>& entries)
        : entries(entries) {}

    bool readOnce(T* output) override {
        if (currentIndex < entries.size()) {
            *output = entries[currentIndex++];
            return true;
        } else {
            return false;
        }
    }

    bool readAll(std::function<void(const T&)> callback) override {
        for (const auto& entry : entries) {
            callback(entry);
        }
        return true;
    }
};

/**
 * @brief Test that streamLogs method streams log entries.
 */
TEST_F(ROMBuildServiceTest, StreamLogsSuccess) {
    BuildAction request;
    request.set_build_id("build-12345");

    std::vector<BuildLogEntry> logEntries;
    BuildLogEntry entry1;
    entry1.set_timestamp(1234567890);
    entry1.set_level(LogLevel::Info);
    entry1.set_message("Build started");
    entry1.set_is_finished(false);
    logEntries.push_back(entry1);

    BuildLogEntry entry2;
    entry2.set_timestamp(1234567900);
    entry2.set_level(LogLevel::Info);
    entry2.set_message("Build completed");
    entry2.set_is_finished(true);
    logEntries.push_back(entry2);

    EXPECT_CALL(*mockService, streamLogs(_))
        .WillOnce(Invoke([&](const BuildAction& request) {
            return std::make_unique<TestRepeatableSource<BuildLogEntry>>(
                logEntries);
        }));
    std::vector<BuildLogEntry> receivedEntries;
    auto logStream = mockService->streamLogs(request);
    ASSERT_NE(logStream, nullptr);
    bool status = logStream->readAll(
        [&](const BuildLogEntry& entry) { receivedEntries.push_back(entry); });
    EXPECT_TRUE(status);
    ASSERT_EQ(receivedEntries.size(), 2);
    EXPECT_EQ(receivedEntries[0].message(), "Build started");
    EXPECT_FALSE(receivedEntries[0].is_finished());
    EXPECT_EQ(receivedEntries[1].message(), "Build completed");
    EXPECT_TRUE(receivedEntries[1].is_finished());
}

/**
 * @brief Test that cancelBuild method works.
 */
TEST_F(ROMBuildServiceTest, CancelBuildSuccess) {
    BuildAction request;
    request.set_build_id("build-12345");

    EXPECT_CALL(*mockService, cancelBuild(_)).WillOnce(Return(true));

    bool result = mockService->cancelBuild(request);

    EXPECT_TRUE(result);
}

/**
 * @brief Test that directoryExists method works.
 */
TEST_F(ROMBuildServiceTest, DirectoryExistsSuccess) {
    CleanDirectoryRequest request;
    request.set_directory_type(CleanDirectoryType::ROMDirectory);

    DirectoryExistsResponse expectedResponse;
    expectedResponse.set_exists(true);

    EXPECT_CALL(*mockService, directoryExists(_, _))
        .WillOnce(DoAll(Invoke([&](const CleanDirectoryRequest& req,
                                   DirectoryExistsResponse* resp) {
                            *resp = expectedResponse;
                        }),
                        Return(true)));

    DirectoryExistsResponse actualResponse;
    bool result = mockService->directoryExists(request, &actualResponse);

    EXPECT_TRUE(result);
    EXPECT_TRUE(actualResponse.exists());
}

/**
 * @brief Test that getBuildResult method streams result chunks.
 */
TEST_F(ROMBuildServiceTest, GetBuildResultSuccess) {
    BuildAction request;
    request.set_build_id("build-12345");

    BuildResult result1;
    result1.set_success(true);
    result1.set_upload_method(UploadMethod::GoFile);
    result1.set_gofile_link("https://gofile.io/d/abc123");
    result1.set_file_name("lineage-19.1-sailfish.zip");

    EXPECT_CALL(*mockService, getBuildResult(_))
        .WillOnce(Invoke([&](const BuildAction& req) {
            return std::make_unique<TestRepeatableSource<BuildResult>>(
                std::vector<BuildResult>{result1});
        }));

    std::string receivedLink;
    std::string receivedFileName;
    bool receivedSuccess = false;
    auto resultStream = mockService->getBuildResult(request);
    ASSERT_NE(resultStream, nullptr);
    bool status = resultStream->readAll([&](const BuildResult& result) {
        receivedSuccess = result.success();
        receivedLink = result.gofile_link();
        receivedFileName = result.file_name();
    });

    EXPECT_TRUE(status);
    EXPECT_TRUE(receivedSuccess);
    EXPECT_EQ(receivedLink, "https://gofile.io/d/abc123");
    EXPECT_EQ(receivedFileName, "lineage-19.1-sailfish.zip");
}

/**
 * @brief Test failure scenarios.
 */
TEST_F(ROMBuildServiceTest, StartBuildFailure) {
    BuildRequest request;
    request.set_config_name("invalid_rom");

    EXPECT_CALL(*mockService, startBuild(_, _)).WillOnce(Return(false));

    BuildSubmission response;
    bool result = mockService->startBuild(request, &response);

    EXPECT_FALSE(result);
}
