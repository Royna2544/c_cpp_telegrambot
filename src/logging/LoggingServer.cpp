#include "LoggingServer.hpp"

#include <absl/log/log_entry.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>
#include <absl/time/time.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <ManagedThreads.hpp>
#include <SharedMalloc.hpp>
#include <algorithm>
#include <atomic>
#include <barrier>
#include <mutex>
#include <stop_token>

#include "LogSinks.hpp"
#include "Log_service.grpc.pb.h"
#include "Log_service.pb.h"

class LogSinkImpl : public absl::LogSink {
    void Send(const absl::LogEntry& entry) override;
    grpc::ServerWriter<tgbot::proto::logging::LogData>* _impl;
    bool _stopped = false;
    std::mutex mu_;

   public:
    // Requires an accepted socket.
    explicit LogSinkImpl(
        grpc::ServerWriter<tgbot::proto::logging::LogData>* impl)
        : _impl(impl) {};

    ~LogSinkImpl() override = default;

    void stop() {
        std::lock_guard<std::mutex> lock(mu_);
        _stopped = true;
    }
};

void LogSinkImpl::Send(const absl::LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mu_);
    tgbot::proto::logging::LogData le{};
    le.set_severity(
        static_cast<tgbot::proto::logging::LogSeverity>(entry.log_severity()));
    le.set_timestamp(absl::ToUnixSeconds(entry.timestamp()));
    le.set_message(entry.text_message());
    bool rc = _impl->Write(le);
    if (!rc) {
        _stopped = true;
    }
}

struct NetworkLogSink::Impl {
    std::unique_ptr<grpc::Server> server;
    std::stop_token token;
};

class LoggingServiceImpl
    : public tgbot::proto::logging::LoggingService::Service {
    NetworkLogSink::Impl* impl;
    grpc::Status getLogs(
        grpc::ServerContext* context,
        const google::protobuf::Empty* /*request*/,
        grpc::ServerWriter<tgbot::proto::logging::LogData>* writer) override {
        LOG(INFO) << "Client connected for log streaming";
        RAIILogSink<LogSinkImpl> sinkGuard(writer);
        // Wait until the client disconnects
        while (!context->IsCancelled() && !impl->token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (context->IsCancelled()) {
            LOG(INFO) << "Client disconnected from log streaming";
        } else {
            LOG(INFO) << "Log streaming stopped by server";
        }
        return grpc::Status::OK;
    }

   public:
    explicit LoggingServiceImpl(NetworkLogSink::Impl* impl) : impl(impl) {}
};

void NetworkLogSink::runFunction(const std::stop_token& token) {
    std::stop_source listen_source;
    std::stop_token listen_token = listen_source.get_token();
    grpc::ServerBuilder builder;
    impl->token = token;
    LoggingServiceImpl service_(impl.get());
    builder.RegisterService(&service_);
    builder.AddListeningPort(url, grpc::InsecureServerCredentials());
    impl->server = builder.BuildAndStart();
    if (!impl->server) {
        LOG(ERROR) << "Failed to start gRPC server for logging";
        return;
    }
    LOG(INFO) << "NetworkLogSink listening on " << url;
    impl->server->Wait();
    LOG(INFO) << "NetworkLogSink stopped listening";
}

void NetworkLogSink::onPreStop() {
    LOG(INFO) << "NetworkLogSink stopping...";
    impl->server->Shutdown();
}

NetworkLogSink::NetworkLogSink(std::string url)
    : url(std::move(url)), impl(std::make_unique<Impl>()) {
    run();
}

NetworkLogSink::~NetworkLogSink() = default;