#pragma once

#include <absl/base/log_severity.h>
#include <absl/log/log_sink.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_split.h>

#include <mutex>

struct FileSinkBase : absl::LogSink {
    void Send(const absl::LogEntry& entry) override {
        const std::lock_guard<std::mutex> lock(m);
        if (entry.log_severity() < absl::LogSeverity::kError) {
            fputs(entry.text_message_with_prefix_and_newline().data(), stdout);
        }
    }
    FileSinkBase() = default;
    ~FileSinkBase() override {
        if (fp_) {
            fclose(fp_);
        }
    }

   protected:
    std::mutex m;
    FILE* fp_ = nullptr;
};

struct LogFileSink : FileSinkBase {
    void init(const std::string& filename) {
        fp_ = fopen(filename.c_str(), "w");
    }
};

struct StdFileSink : FileSinkBase {
    StdFileSink() { fp_ = stdout; }
};