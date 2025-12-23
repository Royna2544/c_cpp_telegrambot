#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

#include <ManagedThreads.hpp>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <utility>

#include "SocketContext.hpp"

struct NetworkLogSink : public ThreadRunner {
    class LogSinkImpl : public spdlog::sinks::base_sink<std::mutex> {
        void sink_it_(const spdlog::details::log_msg& msg) override;
        void flush_() override {}
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
    TgBotSocket::Context* context = nullptr;
};