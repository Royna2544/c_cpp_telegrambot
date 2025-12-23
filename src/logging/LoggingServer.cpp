#include "LoggingServer.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

#include <AbslLogCompat.hpp>
#include <ManagedThreads.hpp>
#include <SharedMalloc.hpp>
#include <algorithm>
#include <mutex>
#include <stop_token>

#include "LogSinks.hpp"
#include "LogcatData.hpp"
#include "SocketContext.hpp"

void NetworkLogSink::LogSinkImpl::sink_it_(const spdlog::details::log_msg& msg) {
    if (_stop.stop_requested()) {
        return;
    }
    LogEntry le{};
    le.severity = msg.level;
    
    // Format the message using the formatter
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    std::string formatted_str = fmt::to_string(formatted);
    
    std::ranges::copy_n(formatted_str.begin(), 
                       std::min(formatted_str.size(), static_cast<size_t>(MAX_LOGMSG_SIZE - 1)), 
                       le.message.begin());
    SharedMalloc logData(le);
    bool ret = false;
    if (context != nullptr) {
        ret = context->write(logData);
        if (!ret) {
            SPDLOG_ERROR("Failed to send log data to client");
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
            return !token.stop_requested() && !sink_token.stop_requested();
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
