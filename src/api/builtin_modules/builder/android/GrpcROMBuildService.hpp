#pragma once

#include <google/protobuf/empty.pb.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <memory>
#include <string>

#include "IROMBuildService.hpp"
#include "ROMBuild_service.grpc.pb.h"

namespace tgbot::builder::android {

/**
 * @brief gRPC-based implementation of the ROM build service interface.
 *
 * This class wraps the generated gRPC stub and implements the
 * IROMBuildService interface, providing the actual communication
 * with the remote build service.
 */
class GrpcROMBuildService : public IROMBuildService {
   public:
    /**
     * @brief Construct a new gRPC ROM Build Service.
     *
     * @param channel The gRPC channel to use for communication.
     */
    explicit GrpcROMBuildService(std::shared_ptr<grpc::Channel> channel)
        : stub_(ROMBuildService::NewStub(channel)) {}

    /**
     * @brief Construct a new gRPC ROM Build Service.
     *
     * @param server_address The address of the gRPC server.
     */
    explicit GrpcROMBuildService(const std::string& server_address)
        : GrpcROMBuildService(grpc::CreateChannel(
              server_address, grpc::InsecureChannelCredentials())) {}

    ~GrpcROMBuildService() override = default;

    bool getSettings(Settings* response) override {
        grpc::ClientContext context;
        google::protobuf::Empty request;
        auto status = stub_->GetSettings(&context, request, response);
        return status.ok();
    }

    bool setSettings(const Settings& settings) override {
        grpc::ClientContext context;
        google::protobuf::Empty response;
        auto status = stub_->SetSettings(&context, settings, &response);
        return status.ok();
    }

    bool cleanDirectory(const CleanDirectoryRequest& request) override {
        grpc::ClientContext context;
        google::protobuf::Empty response;
        auto status = stub_->CleanDirectory(&context, request, &response);
        return status.ok();
    }

    bool directoryExists(const CleanDirectoryRequest& request,
                         DirectoryExistsResponse* response) override {
        grpc::ClientContext context;
        auto status = stub_->DirectoryExists(&context, request, response);
        return status.ok();
    }

    bool startBuild(const BuildRequest& request,
                    BuildSubmission* response) override {
        grpc::ClientContext context;
        auto status = stub_->StartBuild(&context, request, response);
        return status.ok();
    }

    bool streamLogs(
        const BuildAction& request,
        std::function<void(const BuildLogEntry&)> callback) override {
        grpc::ClientContext context;
        auto stream = stub_->StreamLogs(&context, request);
        BuildLogEntry entry;
        while (stream->Read(&entry)) {
            callback(entry);
        }
        return stream->Finish().ok();
    }

    bool cancelBuild(const BuildAction& request) override {
        grpc::ClientContext context;
        google::protobuf::Empty response;
        auto status = stub_->CancelBuild(&context, request, &response);
        return status.ok();
    }

    bool getStatus(const BuildAction& request,
                   BuildSubmission* response) override {
        grpc::ClientContext context;
        auto status = stub_->GetStatus(&context, request, response);
        return status.ok();
    }

    bool getBuildResult(
        const BuildAction& request,
        std::function<void(const BuildResult&)> callback) override {
        grpc::ClientContext context;
        auto stream = stub_->GetBuildResult(&context, request);
        BuildResult result;
        while (stream->Read(&result)) {
            callback(result);
        }
        return stream->Finish().ok();
    }

   private:
    std::unique_ptr<ROMBuildService::Stub> stub_;
};

}  // namespace tgbot::builder::android
