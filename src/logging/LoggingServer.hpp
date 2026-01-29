#pragma once

#include <absl/log/log_entry.h>
#include <absl/log/log_sink.h>

#include <ManagedThreads.hpp>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <utility>

struct NetworkLogSink : public ThreadRunner {
    struct Impl;

    void runFunction(const std::stop_token& token) override;

    void onPreStop() override;

    explicit NetworkLogSink(std::string url);
    ~NetworkLogSink() override;

   private:
    std::unique_ptr<Impl> impl;
    std::string url;
};