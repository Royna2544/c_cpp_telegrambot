#pragma once

#include <absl/base/log_severity.h>
#include <absl/log/log_sink.h>
#include <absl/log/log.h>
#include <StructF.hpp>

#include <mutex>

struct FileSinkBase : absl::LogSink {
    void Send(const absl::LogEntry& entry) override {
        const std::lock_guard<std::mutex> lock(m);
        if (entry.log_severity() < absl::LogSeverity::kError) {
            file.puts(entry.text_message_with_prefix_and_newline());
        }
    }
    FileSinkBase() = default;
    ~FileSinkBase() override = default;

   protected:
    std::mutex m;
    F file;
};

struct LogFileSink : FileSinkBase {
    void init(const std::string& filename) {
        const auto& res = file.open(filename, F::Mode::Write);
        if (!res) {
            LOG(ERROR) << "Couldn't open file " << filename << ": " << res.reason;
            file = nullptr;
        }
    }
};

struct StdFileSink : FileSinkBase {
    StdFileSink() { file = std::move(StderrF()); }
};