#pragma once

#include <absl/log/log.h>
#include <google/protobuf/empty.pb.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/support/status.h>

#include <memory>
#include <string>

#include "IROMBuildService.hpp"
#include "ROMBuild_service.grpc.pb.h"
#include "ROMBuild_service.pb.h"

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
    template <typename T>
    struct GrpcRepeatableSource : public RepeatableSource<T> {
        explicit GrpcRepeatableSource(
            std::unique_ptr<grpc::ClientReader<T>> stream,
            std::unique_ptr<grpc::ClientContext> context)
            : stream_(std::move(stream)), context_(std::move(context)) {}
        ~GrpcRepeatableSource() override = default;

        bool readOnce(T* output) override { return stream_->Read(output); }

        bool readAll(std::function<void(const T&)> callback) override {
            T entry;
            bool anyRead = false;
            while (stream_->Read(&entry)) {
                callback(entry);
                anyRead = true;
            }
            return anyRead;
        }

        [[nodiscard]] grpc::Status finish() { return stream_->Finish(); }

       private:
        std::unique_ptr<grpc::ClientReader<T>> stream_;
        std::unique_ptr<grpc::ClientContext> context_;
    };

    /**
     * @brief Construct a new gRPC ROM Build Service.
     *
     * @param channel The gRPC channel to use for communication.
     */
    explicit GrpcROMBuildService(const std::shared_ptr<grpc::Channel>& channel)
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
        LOG_IF(ERROR, !status.ok())
            << "GetSettings RPC failed: " << status.error_message();
        return status.ok();
    }

    bool setSettings(const Settings& settings) override {
        grpc::ClientContext context;
        google::protobuf::Empty response;
        auto status = stub_->SetSettings(&context, settings, &response);
        LOG_IF(ERROR, !status.ok())
            << "SetSettings RPC failed: " << status.error_message();
        return status.ok();
    }

    bool cleanDirectory(const CleanDirectoryRequest& request) override {
        grpc::ClientContext context;
        google::protobuf::Empty response;
        auto status = stub_->CleanDirectory(&context, request, &response);
        LOG_IF(ERROR, !status.ok())
            << "CleanDirectory RPC failed: " << status.error_message();
        return status.ok();
    }

    bool directoryExists(const CleanDirectoryRequest& request,
                         DirectoryExistsResponse* response) override {
        grpc::ClientContext context;
        auto status = stub_->DirectoryExists(&context, request, response);
        LOG_IF(ERROR, !status.ok())
            << "DirectoryExists RPC failed: " << status.error_message();
        return status.ok();
    }

    bool startBuild(const BuildRequest& request,
                    BuildSubmission* response) override {
        grpc::ClientContext context;
        auto status = stub_->StartBuild(&context, request, response);
        LOG_IF(ERROR, !status.ok())
            << "StartBuild RPC failed: " << status.error_message();
        return status.ok();
    }

    std::unique_ptr<RepeatableSource<BuildLogEntry>> streamLogs(
        const BuildAction& request) override {
        std::unique_ptr<grpc::ClientContext> context =
            std::make_unique<grpc::ClientContext>();
        auto stream = stub_->StreamLogs(context.get(), request);
        if (!stream) {
            LOG(ERROR) << "StreamLogs RPC failed to start.";
            return nullptr;
        }
        return std::make_unique<GrpcRepeatableSource<BuildLogEntry>>(
            std::move(stream), std::move(context));
    }

    bool cancelBuild(const BuildAction& request) override {
        grpc::ClientContext context;
        google::protobuf::Empty response;
        auto status = stub_->CancelBuild(&context, request, &response);
        LOG_IF(ERROR, !status.ok())
            << "CancelBuild RPC failed: " << status.error_message();
        return status.ok();
    }

    bool getStatus(const BuildAction& request,
                   BuildSubmission* response) override {
        grpc::ClientContext context;
        auto status = stub_->GetStatus(&context, request, response);
        LOG_IF(ERROR, !status.ok())
            << "GetStatus RPC failed: " << status.error_message();
        return status.ok();
    }

    std::unique_ptr<RepeatableSource<BuildResult>> getBuildResult(
        const BuildAction& request) override {
        std::unique_ptr<grpc::ClientContext> context =
            std::make_unique<grpc::ClientContext>();
        auto stream = stub_->GetBuildResult(context.get(), request);
        if (!stream) {
            LOG(ERROR) << "GetBuildResult RPC failed to start.";
            return nullptr;
        }
        return std::make_unique<GrpcRepeatableSource<BuildResult>>(
            std::move(stream), std::move(context));
    }

   private:
    std::unique_ptr<ROMBuildService::Stub> stub_;
};

}  // namespace tgbot::builder::android
