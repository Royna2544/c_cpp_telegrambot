#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <trivial_helpers/_class_helper_macros.h>

#include <StructF.hpp>
#include <AbslLogCompat.hpp>
#include <concepts>
#include <filesystem>
#include <memory>
#include <mutex>
#include <type_traits>

template <typename Mutex>
using base_sink = spdlog::sinks::base_sink<Mutex>;

template <std::derived_from<base_sink<std::mutex>> Sink>
struct RAIILogSink {
    explicit RAIILogSink()
        requires std::is_default_constructible_v<Sink>
        : _sink(std::make_shared<Sink>()) {
        spdlog::default_logger()->sinks().push_back(_sink);
    }
    template <typename... Args>
    explicit RAIILogSink(Args&&... args)
        requires std::is_constructible_v<Sink, Args...> &&
                 (sizeof...(Args) != 0)
        : _sink(std::make_shared<Sink>(args...)) {
        spdlog::default_logger()->sinks().push_back(_sink);
    }
    RAIILogSink() = default;
    ~RAIILogSink() {
        if (_sink) {
            auto& sinks = spdlog::default_logger()->sinks();
            sinks.erase(std::remove(sinks.begin(), sinks.end(), _sink), sinks.end());
        }
    }

    RAIILogSink& operator=(std::shared_ptr<Sink>&& sink) & {
        if (_sink) {
            auto& sinks = spdlog::default_logger()->sinks();
            sinks.erase(std::remove(sinks.begin(), sinks.end(), _sink), sinks.end());
        }
        _sink = std::move(sink);
        if (_sink) {
            spdlog::default_logger()->sinks().push_back(_sink);
        }
        return *this;
    }

    NO_COPY_CTOR(RAIILogSink);

   private:
    std::shared_ptr<Sink> _sink;
};

struct StdFileSink : base_sink<std::mutex> {
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (msg.level < spdlog::level::err) {
            spdlog::memory_buf_t formatted;
            base_sink<std::mutex>::formatter_->format(msg, formatted);
            (void)file.puts(fmt::to_string(formatted).c_str());
        }
    }
    
    void flush_() override {
        // F class handles flushing internally
    }
    
    StdFileSink() : file(StderrF()) {}
    ~StdFileSink() override = default;

   private:
    F file;
};

struct LogFileSink : base_sink<std::mutex> {
    explicit LogFileSink(std::filesystem::path filename) {
        const auto& res = file.open(filename.string(), F::Mode::Write);
        if (!res) {
            SPDLOG_ERROR("Couldn't open file {}: {}", filename.string(), res.reason);
            file = nullptr;
        }
        SPDLOG_INFO("File {} added as logsink", filename.string());
    }

    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        base_sink<std::mutex>::formatter_->format(msg, formatted);
        (void)file.puts(fmt::to_string(formatted).c_str());
    }
    
    void flush_() override {
        // F class handles flushing internally
    }

    ~LogFileSink() override = default;

   private:
    F file;
};

constexpr std::string_view kDefaultLogFile = "tgbot." BUILD_TYPE_STR ".log";
