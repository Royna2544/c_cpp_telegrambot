#pragma once

#include <grpcpp/client_context.h>
#include <grpcpp/support/status.h>
#include <grpcpp/support/sync_stream.h>

#include <functional>
#include <memory>
#include <utility>

namespace tgbot::builder {

/**
 * @brief Caller-owned handle over a server-streaming gRPC response.
 *
 * Unlike a baked-in read loop, a RepeatableSource lets the caller drive the
 * stream: peek the first message (@ref readOnce), drain the rest
 * (@ref readAll), and retrieve the final @c grpc::Status (@ref finish).
 *
 * Contract: @ref readAll does NOT call @c Finish; callers must call
 * @ref finish exactly once after the stream is drained to obtain the status.
 * Calling @ref finish more than once is undefined behaviour.
 */
template <typename T>
struct RepeatableSource {
    RepeatableSource() = default;
    virtual ~RepeatableSource() = default;

    /// Read a single message. Returns false when the stream is exhausted.
    virtual bool readOnce(T* output) = 0;

    /// Drain the stream, invoking @p callback for each message.
    /// Returns true if at least one message was read.
    virtual bool readAll(std::function<void(const T&)> callback) = 0;

    /// Block until the stream completes and return its terminal status.
    [[nodiscard]] virtual grpc::Status finish() = 0;

    RepeatableSource(const RepeatableSource&) = delete;
    RepeatableSource& operator=(const RepeatableSource&) = delete;
    RepeatableSource(RepeatableSource&&) = delete;
    RepeatableSource& operator=(RepeatableSource&&) = delete;
};

/**
 * @brief gRPC-backed RepeatableSource.
 *
 * Owns both the reader and its ClientContext so the stream outlives the call
 * that created it (the context must remain alive for the lifetime of reads).
 */
template <typename T>
class GrpcRepeatableSource : public RepeatableSource<T> {
   public:
    GrpcRepeatableSource(std::unique_ptr<grpc::ClientReader<T>> stream,
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

    [[nodiscard]] grpc::Status finish() override { return stream_->Finish(); }

   private:
    std::unique_ptr<grpc::ClientReader<T>> stream_;
    std::unique_ptr<grpc::ClientContext> context_;
};

}  // namespace tgbot::builder
