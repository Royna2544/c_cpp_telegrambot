#pragma once

#include <absl/log/log.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <memory>
#include <string>
#include <utility>

#include "GrpcStream.hpp"
#include "ILinuxKernelBuildService.hpp"
#include "LinuxKernelBuild_service.grpc.pb.h"

namespace tgbot::builder::linuxkernel {

/**
 * @brief gRPC-based implementation of the Linux kernel build service interface.
 *
 * Wraps the generated gRPC stub. Streaming RPCs return a
 * @ref GrpcRepeatableSource that owns its ClientContext, so the stream may be
 * read by the caller after this method returns.
 */
class GrpcLinuxKernelBuildService : public ILinuxKernelBuildService {
   public:
    explicit GrpcLinuxKernelBuildService(
        const std::shared_ptr<grpc::Channel>& channel)
        : stub_(LinuxKernelBuildService::NewStub(channel)) {}

    explicit GrpcLinuxKernelBuildService(const std::string& server_address)
        : GrpcLinuxKernelBuildService(grpc::CreateChannel(
              server_address, grpc::InsecureChannelCredentials())) {}

    ~GrpcLinuxKernelBuildService() override = default;

    bool updateConfig(const Config& request,
                      ConfigResponse* response) override {
        grpc::ClientContext context;
        auto status = stub_->updateConfig(&context, request, response);
        LOG_IF(ERROR, !status.ok())
            << "updateConfig RPC failed: " << status.error_message();
        return status.ok();
    }

    std::unique_ptr<RepeatableSource<BuildStatus>> prepareBuild(
        const BuildPrepareRequest& request) override {
        auto context = std::make_unique<grpc::ClientContext>();
        auto stream = stub_->prepareBuild(context.get(), request);
        if (!stream) {
            LOG(ERROR) << "prepareBuild RPC failed to start.";
            return nullptr;
        }
        return std::make_unique<GrpcRepeatableSource<BuildStatus>>(
            std::move(stream), std::move(context));
    }

    std::unique_ptr<RepeatableSource<BuildStatus>> doBuild(
        const BuildRequest& request) override {
        auto context = std::make_unique<grpc::ClientContext>();
        auto stream = stub_->doBuild(context.get(), request);
        if (!stream) {
            LOG(ERROR) << "doBuild RPC failed to start.";
            return nullptr;
        }
        return std::make_unique<GrpcRepeatableSource<BuildStatus>>(
            std::move(stream), std::move(context));
    }

    bool cancelBuild(const BuildRequest& request,
                     BuildStatus* response) override {
        grpc::ClientContext context;
        auto status = stub_->cancelBuild(&context, request, response);
        LOG_IF(ERROR, !status.ok())
            << "cancelBuild RPC failed: " << status.error_message();
        return status.ok();
    }

    std::unique_ptr<RepeatableSource<ArtifactChunk>> getArtifact(
        const BuildRequest& request) override {
        auto context = std::make_unique<grpc::ClientContext>();
        auto stream = stub_->getArtifact(context.get(), request);
        if (!stream) {
            LOG(ERROR) << "getArtifact RPC failed to start.";
            return nullptr;
        }
        return std::make_unique<GrpcRepeatableSource<ArtifactChunk>>(
            std::move(stream), std::move(context));
    }

   private:
    std::unique_ptr<LinuxKernelBuildService::Stub> stub_;
};  // class GrpcLinuxKernelBuildService

}  // namespace tgbot::builder::linuxkernel
