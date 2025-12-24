#pragma once

#include <absl/log/log_entry.h>
#include <absl/log/log_sink.h>

#include <ManagedThreads.hpp>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include "SocketContext.hpp"

struct NetworkLogSink : public ThreadRunner {
    class LogSinkImpl : public absl::LogSink {
        void Send(const absl::LogEntry& entry) override;
        const TgBotSocket::Context* context = nullptr;
        std::stop_source _stop;

       public:
        // Requires an accepted socket.
        explicit LogSinkImpl(const TgBotSocket::Context* context,
                             std::stop_source source)
            : context(context), _stop(std::move(source)){};
    };

    void runFunction(const std::stop_token& token) override;

    void onPreStop() override;

    explicit NetworkLogSink(TgBotSocket::Context* context);

   private:
    void handleClient(const TgBotSocket::Context& c, std::stop_token token);
    
    TgBotSocket::Context* context = nullptr;
    std::vector<std::jthread> client_threads;
    std::mutex threads_mutex;
};