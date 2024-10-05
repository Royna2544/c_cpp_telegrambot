#include <absl/log/initialize.h>
#include <absl/log/log_sink_registry.h>
#include "../include/LogSinks.hpp"

static StdFileSink sink;

void TgBot_AbslLogInit() {    
    absl::InitializeLog();
    absl::AddLogSink(&sink);
}

void TgBot_AbslLogDeInit() {
    absl::RemoveLogSink(&sink);
}