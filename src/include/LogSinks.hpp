#pragma once

#include <absl/base/log_severity.h>
#include <absl/log/log.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>
#include <trivial_helpers/_class_helper_macros.h>

#include <StructF.hpp>
#include <concepts>
#include <memory>
#include <mutex>
#include <type_traits>

template <std::derived_from<absl::LogSink> Sink>
struct RAIILogSink {
    explicit RAIILogSink()
        requires std::is_default_constructible_v<Sink>
        : _sink(std::make_unique<Sink>()) {
        absl::AddLogSink(_sink.get());
    }
    template <typename... Args>
    explicit RAIILogSink(Args&&... args)
        requires std::is_constructible_v<Sink, Args...> &&
                 (sizeof...(Args) != 0)
        : _sink(std::make_unique<Sink>(args...)) {
        absl::AddLogSink(_sink.get());
    }
    RAIILogSink() = default;
    ~RAIILogSink() {
        if (_sink) {
            absl::RemoveLogSink(_sink.get());
        }
    }

    RAIILogSink& operator=(std::unique_ptr<Sink>&& sink) & {
        if (_sink) {
            absl::RemoveLogSink(_sink.get());
        }
        _sink = std::move(sink);
        if (_sink) {
            absl::AddLogSink(_sink.get());
        }
        return *this;
    }

    NO_COPY_CTOR(RAIILogSink);

   private:
    std::unique_ptr<Sink> _sink;
};

struct FileSinkBase : absl::LogSink {
    void Send(const absl::LogEntry& entry) override {
        const std::lock_guard<std::mutex> lock(m);
        if (entry.log_severity() < absl::LogSeverity::kError) {
            (void)file.puts(
                entry.text_message_with_prefix_and_newline().data());
        }
    }
    FileSinkBase() = default;
    ~FileSinkBase() override = default;

   protected:
    std::mutex m;
    F file;
};

struct LogFileSink : FileSinkBase {
    explicit LogFileSink(std::string_view filename) {
        const auto& res = file.open(filename, F::Mode::Write);
        if (!res) {
            LOG(ERROR) << "Couldn't open file " << filename << ": "
                       << res.reason;
            file = nullptr;
        }
    }
};

struct StdFileSink : FileSinkBase {
    StdFileSink() noexcept { file = std::move(StderrF()); }
};