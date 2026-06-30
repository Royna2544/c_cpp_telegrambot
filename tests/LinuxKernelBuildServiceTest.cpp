#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "mocks/LinuxKernelBuildService.hpp"

using namespace tgbot::builder::linuxkernel;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;

namespace {

/// In-memory RepeatableSource used to fake streaming RPC responses.
template <typename T>
struct TestRepeatableSource : public RepeatableSource<T> {
    std::vector<T> entries;
    std::size_t index = 0;

    explicit TestRepeatableSource(std::vector<T> e) : entries(std::move(e)) {}

    bool readOnce(T* output) override {
        if (index >= entries.size()) {
            return false;
        }
        *output = entries[index++];
        return true;
    }

    bool readAll(std::function<void(const T&)> callback) override {
        bool anyRead = false;
        while (index < entries.size()) {
            callback(entries[index++]);
            anyRead = true;
        }
        return anyRead;
    }

    grpc::Status finish() override { return grpc::Status::OK; }
};

}  // namespace

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
 * @brief Test that prepareBuild returns a stream of status updates.
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

    EXPECT_CALL(*mockService, prepareBuild(_))
        .WillOnce(Invoke([&](const BuildPrepareRequest& req) {
            return std::make_unique<TestRepeatableSource<BuildStatus>>(
                statusUpdates);
        }));

    auto stream = mockService->prepareBuild(request);
    ASSERT_NE(stream, nullptr);

    std::vector<BuildStatus> receivedStatuses;
    stream->readAll(
        [&](const BuildStatus& status) { receivedStatuses.push_back(status); });
    EXPECT_TRUE(stream->finish().ok());

    ASSERT_EQ(receivedStatuses.size(), 2);
    EXPECT_EQ(receivedStatuses[0].status(),
              ProgressStatus::IN_PROGRESS_DOWNLOAD);
    EXPECT_EQ(receivedStatuses[1].status(), ProgressStatus::SUCCESS);
    EXPECT_EQ(receivedStatuses[1].build_id(), 42);
}

/**
 * @brief Test that doBuild returns a stream.
 */
TEST_F(LinuxKernelBuildServiceTest, DoBuildSuccess) {
    BuildRequest request;
    request.set_build_id(42);

    BuildStatus finalStatus;
    finalStatus.set_status(ProgressStatus::SUCCESS);
    finalStatus.set_output("Build completed successfully");

    EXPECT_CALL(*mockService, doBuild(_))
        .WillOnce(Invoke([&](const BuildRequest& req) {
            return std::make_unique<TestRepeatableSource<BuildStatus>>(
                std::vector<BuildStatus>{finalStatus});
        }));

    auto stream = mockService->doBuild(request);
    ASSERT_NE(stream, nullptr);

    int callbackCount = 0;
    stream->readAll([&](const BuildStatus& status) {
        callbackCount++;
        EXPECT_EQ(status.status(), ProgressStatus::SUCCESS);
    });
    EXPECT_TRUE(stream->finish().ok());
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
 * @brief Test that getArtifact streams metadata followed by data chunks.
 */
TEST_F(LinuxKernelBuildServiceTest, GetArtifactSuccess) {
    BuildRequest request;
    request.set_build_id(42);

    ArtifactChunk metadataChunk;
    metadataChunk.mutable_metadata()->set_filename("kernel.zip");
    metadataChunk.mutable_metadata()->set_total_size(1024);

    ArtifactChunk dataChunk;
    dataChunk.set_data("test data content");

    std::vector<ArtifactChunk> chunks{metadataChunk, dataChunk};
    EXPECT_CALL(*mockService, getArtifact(_))
        .WillOnce(Invoke([&](const BuildRequest& req) {
            return std::make_unique<TestRepeatableSource<ArtifactChunk>>(chunks);
        }));

    auto stream = mockService->getArtifact(request);
    ASSERT_NE(stream, nullptr);

    // First message carries metadata.
    ArtifactChunk first;
    ASSERT_TRUE(stream->readOnce(&first));
    ASSERT_TRUE(first.has_metadata());
    EXPECT_EQ(first.metadata().filename(), "kernel.zip");

    std::string receivedData;
    stream->readAll([&](const ArtifactChunk& chunk) {
        if (chunk.has_data()) {
            receivedData += chunk.data();
        }
    });
    EXPECT_TRUE(stream->finish().ok());
    EXPECT_EQ(receivedData, "test data content");
}

/**
 * @brief Test failure scenario: a null stream is returned.
 */
TEST_F(LinuxKernelBuildServiceTest, PrepareBuildFailure) {
    BuildPrepareRequest request;
    request.set_name("invalid_kernel");

    EXPECT_CALL(*mockService, prepareBuild(_))
        .WillOnce(Invoke([](const BuildPrepareRequest&) {
            return std::unique_ptr<RepeatableSource<BuildStatus>>(nullptr);
        }));

    auto stream = mockService->prepareBuild(request);
    EXPECT_EQ(stream, nullptr);
}
