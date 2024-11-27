#include <absl/log/initialize.h>
#include <absl/log/log_sink_registry.h>

#include <optional>

#include "../include/LogSinks.hpp"

static std::optional<StdFileSink> sink;

void TgBot_AbslLogInit() {
    absl::InitializeLog();
    sink.emplace();
    absl::AddLogSink(&*sink);
}

void TgBot_AbslLogDeInit() {
    if (!sink) {
        return;
    }
    absl::RemoveLogSink(&*sink);
    sink.reset();
}