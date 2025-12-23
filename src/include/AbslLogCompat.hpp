#pragma once

/**
 * Compatibility layer for transitioning from Abseil logging to spdlog
 * This header provides macros that map Abseil logging calls to spdlog equivalents
 */

#include <spdlog/spdlog.h>
#include <cstdlib>

// Map LOG macros to spdlog equivalents
#define LOG(severity) SPDLOG_##severity

// Map DLOG (debug log) macros
#ifdef NDEBUG
  #define DLOG(severity) if (false) SPDLOG_##severity
#else
  #define DLOG(severity) SPDLOG_##severity
#endif

// Map CHECK macros to assertions with logging
#define CHECK(condition) \
  if (!(condition)) { \
    SPDLOG_CRITICAL("Check failed: {}", #condition); \
    std::abort(); \
  }

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))
#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))

// Map severity levels to spdlog
#define INFO info
#define WARNING warn
#define ERROR error
#define FATAL critical
