#pragma once

#include <absl/base/log_severity.h>
#include <absl/log/log_sink.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_split.h>

struct FileSinkBase : absl::LogSink {
    void Send(const absl::LogEntry& entry) override {
        for (absl::string_view line : absl::StrSplit(
                 entry.text_message_with_prefix(), absl::ByChar('\n'))) {
            if (entry.log_severity() == absl::LogSeverity::kInfo) {
                absl::FPrintF(fp_, "%s\r", line);
                fputc('\n', fp_);
            }
        }
    }
    FileSinkBase() = default;
    ~FileSinkBase() override {
        if (fp_) {
            fclose(fp_);
        }
    }

   protected:
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