#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <absl/log/initialize.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>
#include <absl/strings/str_split.h>
#include <mutex>
#include <Windows.h>

struct WindowSinkBase : absl::LogSink {
    void Send(const absl::LogEntry& entry) override {
        for (absl::string_view line : absl::StrSplit(
                 entry.text_message_with_prefix(), absl::ByChar('\n'))) {
            const std::lock_guard<std::mutex> lock(m);
            OutputDebugStringA(line.data());
        }
    }
    WindowSinkBase() = default;
    ~WindowSinkBase() override = default;

   protected:
    std::mutex m;
};

inline void initLogging() {
    static WindowSinkBase sink;
    absl::InitializeLog();
    absl::AddLogSink(&sink);
}