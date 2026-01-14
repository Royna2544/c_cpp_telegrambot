#include "LoggingServer.hpp"

#include <absl/log/log_entry.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>

#include <ManagedThreads.hpp>
#include <SharedMalloc.hpp>
#include <algorithm>
#include <mutex>
#include <stop_token>

#include "LogSinks.hpp"
#include "LogcatData.hpp"
#include "SocketContext.hpp"

void NetworkLogSink::LogSinkImpl::Send(const absl::LogEntry& entry) {
    if (_stop.stop_requested()) {
        LOG(INFO) << "LogSinkImpl::Send: Stop requested, not sending log";
        return;
    }
    LogEntry le{};
    le.severity = entry.log_severity();
    const auto& text = entry.text_message();
    const size_t copy_size =
        std::min(text.size(), static_cast<size_t>(MAX_LOGMSG_SIZE - 1));
    std::ranges::copy_n(text.begin(), copy_size, le.message.begin());
    SharedMalloc logData(le);
    bool ret = false;
    if (context != nullptr) {
        ret = context->write(logData);
        if (!ret) {
            LOG(ERROR) << "Failed to send log data to client";
            _stop.request_stop();
        }
    }
}

void NetworkLogSink::runFunction(const std::stop_token& token) {
    std::stop_source listen_source;
    std::stop_token listen_token = listen_source.get_token();

    // Wait for main shutdown signal
    std::unique_lock lock(m);

    context->listen(
        [token, listen_source, this, &lock](const TgBotSocket::Context& c) {
            std::stop_source sink_source;
            RAIILogSink<LogSinkImpl> sink(&c, sink_source);

            // Wait until either main thread or sink requests stop
            cv.wait(lock, [&token, &sink_source] {
                return token.stop_requested() ||
                       sink_source.get_token().stop_requested();
            });
        },
        true);
}

void NetworkLogSink::onPreStop() {
    context->abortConnections();
    cv.notify_all();
}

NetworkLogSink::NetworkLogSink(TgBotSocket::Context* context)
    : context(context) {
    if (context == nullptr) {
        LOG(ERROR) << "Failed to find default socket interface";
        return;
    }
    run();
}
