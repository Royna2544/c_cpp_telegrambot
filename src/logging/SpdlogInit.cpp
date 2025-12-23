#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <memory>
#include <optional>

#include "../include/LogSinks.hpp"

static std::shared_ptr<spdlog::logger> main_logger;

void TgBot_SpdlogInit() {
    if (main_logger) return;
    
    // Create console sink with color support
    auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);
    
    // Create the main logger with the console sink
    main_logger = std::make_shared<spdlog::logger>("tgbot", console_sink);
    main_logger->set_level(spdlog::level::trace);
    
    // Set as default logger
    spdlog::set_default_logger(main_logger);
    
    // Set pattern to match Abseil-like format: [severity] message
    spdlog::set_pattern("[%L] %v");
}

void TgBot_SpdlogDeInit() {
    if (!main_logger) {
        return;
    }
    spdlog::drop_all();
    main_logger.reset();
}
