#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../mocks/LinuxKernelBuildService.hpp"

using namespace tgbot::builder::linuxkernel;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;

/**
 * @brief Test fixture for the Linux Kernel Build Service interface.
 *
 * This test demonstrates how the build service interface enables testing
 * by allowing mock implementations to be injected.
 */
class LinuxKernelBuildServiceTest : public ::testing::Test {
   protected:
    std::shared_ptr<MockLinuxKernelBuildService> mockService;

    void SetUp() override {
        mockService = std::make_shared<MockLinuxKernelBuildService>();
    }
};

/**
 * @brief Test that updateConfig method can be mocked and used.
 */
TEST_F(LinuxKernelBuildServiceTest, UpdateConfigSuccess) {
    Config request;
    request.set_name("test_kernel");
    request.set_json_content("{}");

    ConfigResponse expectedResponse;
    expectedResponse.set_success(true);
    expectedResponse.set_message("Config updated successfully");

    EXPECT_CALL(*mockService, updateConfig(_, _))
        .WillOnce(DoAll(Invoke([&](const Config& req, ConfigResponse* resp) {
                            *resp = expectedResponse;
                        }),
                        Return(true)));

    ConfigResponse actualResponse;
    bool result = mockService->updateConfig(request, &actualResponse);

    EXPECT_TRUE(result);
    EXPECT_TRUE(actualResponse.success());
    EXPECT_EQ(actualResponse.message(), "Config updated successfully");
}

/**
 * @brief Test that prepareBuild method streams status updates.
 */
TEST_F(LinuxKernelBuildServiceTest, PrepareBuildWithStatusUpdates) {
    BuildPrepareRequest request;
    request.set_name("test_kernel");
    request.set_device_name("test_device");

    std::vector<BuildStatus> statusUpdates;
    BuildStatus status1;
    status1.set_status(ProgressStatus::IN_PROGRESS_DOWNLOAD);
    status1.set_output("Downloading sources...");
    status1.set_build_id(42);
    statusUpdates.push_back(status1);

    BuildStatus status2;
    status2.set_status(ProgressStatus::SUCCESS);
    status2.set_output("Preparation complete");
    status2.set_build_id(42);
    statusUpdates.push_back(status2);

    EXPECT_CALL(*mockService, prepareBuild(_, _))
        .WillOnce(Invoke([&](const BuildPrepareRequest& req,
                             std::function<void(const BuildStatus&)> callback) {
            for (const auto& status : statusUpdates) {
                callback(status);
            }
            return true;
        }));

    std::vector<BuildStatus> receivedStatuses;
    bool result = mockService->prepareBuild(
        request,
        [&](const BuildStatus& status) { receivedStatuses.push_back(status); });

    EXPECT_TRUE(result);
    ASSERT_EQ(receivedStatuses.size(), 2);
    EXPECT_EQ(receivedStatuses[0].status(),
              ProgressStatus::IN_PROGRESS_DOWNLOAD);
    EXPECT_EQ(receivedStatuses[1].status(), ProgressStatus::SUCCESS);
    EXPECT_EQ(receivedStatuses[1].build_id(), 42);
}

/**
 * @brief Test that doBuild method can be mocked.
 */
TEST_F(LinuxKernelBuildServiceTest, DoBuildSuccess) {
    BuildRequest request;
    request.set_build_id(42);

    BuildStatus finalStatus;
    finalStatus.set_status(ProgressStatus::SUCCESS);
    finalStatus.set_output("Build completed successfully");

    EXPECT_CALL(*mockService, doBuild(_, _))
        .WillOnce(Invoke([&](const BuildRequest& req,
                             std::function<void(const BuildStatus&)> callback) {
            callback(finalStatus);
            return true;
        }));

    int callbackCount = 0;
    bool result = mockService->doBuild(request, [&](const BuildStatus& status) {
        callbackCount++;
        EXPECT_EQ(status.status(), ProgressStatus::SUCCESS);
    });

    EXPECT_TRUE(result);
    EXPECT_EQ(callbackCount, 1);
}

/**
 * @brief Test that cancelBuild method works.
 */
TEST_F(LinuxKernelBuildServiceTest, CancelBuildSuccess) {
    BuildRequest request;
    request.set_build_id(42);

    BuildStatus expectedResponse;
    expectedResponse.set_status(ProgressStatus::FAILED);
    expectedResponse.set_output("Build cancelled by user");

    EXPECT_CALL(*mockService, cancelBuild(_, _))
        .WillOnce(DoAll(Invoke([&](const BuildRequest& req, BuildStatus* resp) {
                            *resp = expectedResponse;
                        }),
                        Return(true)));

    BuildStatus actualResponse;
    bool result = mockService->cancelBuild(request, &actualResponse);

    EXPECT_TRUE(result);
    EXPECT_EQ(actualResponse.status(), ProgressStatus::FAILED);
    EXPECT_EQ(actualResponse.output(), "Build cancelled by user");
}

/**
 * @brief Test that getArtifact method streams artifact chunks.
 */
TEST_F(LinuxKernelBuildServiceTest, GetArtifactSuccess) {
    BuildRequest request;
    request.set_build_id(42);

    // Create chunks with proper ownership
    ArtifactChunk metadataChunk;
    metadataChunk.mutable_metadata()->set_filename("kernel.zip");
    metadataChunk.mutable_metadata()->set_total_size(1024);

    ArtifactChunk dataChunk;
    dataChunk.set_data("test data content");

    EXPECT_CALL(*mockService, getArtifact(_, _))
        .WillOnce(
            Invoke([&](const BuildRequest& req,
                       std::function<void(const ArtifactChunk&)> callback) {
                callback(metadataChunk);
                callback(dataChunk);
                return true;
            }));

    std::string receivedFilename;
    std::string receivedData;
    bool result =
        mockService->getArtifact(request, [&](const ArtifactChunk& chunk) {
            if (chunk.has_metadata()) {
                receivedFilename = chunk.metadata().filename();
            }
            if (chunk.has_data()) {
                receivedData += chunk.data();
            }
        });

    EXPECT_TRUE(result);
    EXPECT_EQ(receivedFilename, "kernel.zip");
    EXPECT_EQ(receivedData, "test data content");
}

/**
 * @brief Test failure scenarios.
 */
TEST_F(LinuxKernelBuildServiceTest, PrepareBuildFailure) {
    BuildPrepareRequest request;
    request.set_name("invalid_kernel");

    EXPECT_CALL(*mockService, prepareBuild(_, _)).WillOnce(Return(false));

    bool result =
        mockService->prepareBuild(request, [](const BuildStatus& status) {
            // Should not be called on failure
            FAIL() << "Callback should not be invoked on failure";
        });

    EXPECT_FALSE(result);
}
