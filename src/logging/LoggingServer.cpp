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
        return;
    }
    LogEntry le{};
    le.severity = entry.log_severity();
    std::ranges::copy_n(entry.text_message().begin(), MAX_LOGMSG_SIZE - 1, le.message.begin());
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
    context->listen([token](const TgBotSocket::Context& c) {
        std::stop_source source;
        std::stop_token sink_token = source.get_token();
        RAIILogSink<LogSinkImpl> sink(&c, source);
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        std::condition_variable condVariable;

        condVariable.wait(lock, [&token, &sink_token] {
            return token.stop_requested() || sink_token.stop_requested();
        });
    });
}

void NetworkLogSink::onPreStop() { context->abortConnections(); }

NetworkLogSink::NetworkLogSink(TgBotSocket::Context* context)
    : context(context) {
    if (context == nullptr) {
        LOG(ERROR) << "Failed to find default socket interface";
        return;
    }
    run();
}
