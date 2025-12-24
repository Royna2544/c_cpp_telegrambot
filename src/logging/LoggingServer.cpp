#include "LoggingServer.hpp"

#include <absl/log/log_entry.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>

#include <ManagedThreads.hpp>
#include <SharedMalloc.hpp>
#include <algorithm>
#include <mutex>
#include <stop_token>
#include <thread>

#include "LogSinks.hpp"
#include "LogcatData.hpp"
#include "SocketContext.hpp"

void NetworkLogSink::LogSinkImpl::Send(const absl::LogEntry& entry) {
    if (_stop.stop_requested()) {
        return;
    }
    LogEntry le{};
    le.severity = entry.log_severity();
    const auto& text = entry.text_message();
    const size_t copy_size = std::min(text.size(), static_cast<size_t>(MAX_LOGMSG_SIZE - 1));
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

void NetworkLogSink::handleClient(const TgBotSocket::Context& c, std::stop_token token) {
    LOG(INFO) << "New logging client connected from " << c.remoteAddress();
    
    std::stop_source source;
    std::stop_token sink_token = source.get_token();
    RAIILogSink<LogSinkImpl> sink(&c, source);
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    std::condition_variable condVariable;

    condVariable.wait(lock, [&token, &sink_token] {
        return token.stop_requested() || sink_token.stop_requested();
    });
    
    LOG(INFO) << "Logging client disconnected from " << c.remoteAddress();
}

void NetworkLogSink::runFunction(const std::stop_token& token) {
    context->listen([this, token](const TgBotSocket::Context& c) {
        // Spawn a new thread for each client connection
        std::lock_guard<std::mutex> lock(threads_mutex);
        
        // Clean up finished threads
        client_threads.erase(
            std::remove_if(client_threads.begin(), client_threads.end(),
                          [](std::jthread& t) { return !t.joinable(); }),
            client_threads.end());
        
        // Start new client handler thread - capture c by value to avoid dangling reference
        client_threads.emplace_back([this, c, token]() {
            handleClient(c, token);
        });
    });
}

void NetworkLogSink::onPreStop() { 
    context->abortConnections();
    
    // Wait for all client threads to finish
    std::lock_guard<std::mutex> lock(threads_mutex);
    client_threads.clear();  // This will join all threads
}

NetworkLogSink::NetworkLogSink(TgBotSocket::Context* context)
    : context(context) {
    if (context == nullptr) {
        LOG(ERROR) << "Failed to find default socket interface";
        return;
    }
    run();
}
