#pragma once

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <memory>
#include <string>

#include "ILinuxKernelBuildService.hpp"
#include "LinuxKernelBuild_service.grpc.pb.h"

namespace tgbot::builder::linuxkernel {

/**
 * @brief gRPC-based implementation of the Linux kernel build service interface.
 *
 * This class wraps the generated gRPC stub and implements the
 * ILinuxKernelBuildService interface, providing the actual communication
 * with the remote build service.
 */
class GrpcLinuxKernelBuildService : public ILinuxKernelBuildService {
   public:
    /**
     * @brief Construct a new gRPC Linux Kernel Build Service.
     *
     * @param channel The gRPC channel to use for communication.
     */
    explicit GrpcLinuxKernelBuildService(std::shared_ptr<grpc::Channel> channel)
        : stub_(LinuxKernelBuildService::NewStub(channel)) {}

    /**
     * @brief Construct a new gRPC Linux Kernel Build Service.
     *
     * @param server_address The address of the gRPC server.
     */
    explicit GrpcLinuxKernelBuildService(const std::string& server_address)
        : GrpcLinuxKernelBuildService(grpc::CreateChannel(
              server_address, grpc::InsecureChannelCredentials())) {}

    ~GrpcLinuxKernelBuildService() override = default;

    bool updateConfig(const Config& request,
                      ConfigResponse* response) override {
        grpc::ClientContext context;
        auto status = stub_->updateConfig(&context, request, response);
        return status.ok();
    }

    bool prepareBuild(
        const BuildPrepareRequest& request,
        std::function<void(const BuildStatus&)> callback) override {
        grpc::ClientContext context;
        auto stream = stub_->prepareBuild(&context, request);
        BuildStatus status;
        while (stream->Read(&status)) {
            callback(status);
        }
        return stream->Finish().ok();
    }

    bool doBuild(const BuildRequest& request,
                 std::function<void(const BuildStatus&)> callback) override {
        grpc::ClientContext context;
        auto stream = stub_->doBuild(&context, request);
        BuildStatus status;
        while (stream->Read(&status)) {
            callback(status);
        }
        return stream->Finish().ok();
    }

    bool cancelBuild(const BuildRequest& request,
                     BuildStatus* response) override {
        grpc::ClientContext context;
        auto status = stub_->cancelBuild(&context, request, response);
        return status.ok();
    }

    bool getArtifact(
        const BuildRequest& request,
        std::function<void(const ArtifactChunk&)> callback) override {
        grpc::ClientContext context;
        auto stream = stub_->getArtifact(&context, request);
        ArtifactChunk chunk;
        while (stream->Read(&chunk)) {
            callback(chunk);
        }
        return stream->Finish().ok();
    }

   private:
    std::unique_ptr<LinuxKernelBuildService::Stub> stub_;
};

}  // namespace tgbot::builder::linuxkernel
