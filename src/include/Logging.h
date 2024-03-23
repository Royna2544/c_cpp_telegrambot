#pragma once

#include <assert.h>

#include <cstdio>
#include <cstring>
#include <source_location>

#include "EnumArrayHelpers.h"

#ifdef ERROR
#undef ERROR
#endif

enum class LogLevel { FATAL, ERROR, WARNING, INFO, DEBUG, VERBOSE, MAX };

#define ENUM_AND_STR_LOGLEVEL(e) \
    array_helpers::make_elem<LogLevel, const char*>(LogLevel::e, #e)

static inline constexpr auto LogLevelStrMap =
    array_helpers::make<static_cast<int>(LogLevel::MAX), LogLevel, const char*>(
        ENUM_AND_STR_LOGLEVEL(FATAL), ENUM_AND_STR_LOGLEVEL(ERROR),
        ENUM_AND_STR_LOGLEVEL(WARNING), ENUM_AND_STR_LOGLEVEL(INFO),
        ENUM_AND_STR_LOGLEVEL(DEBUG), ENUM_AND_STR_LOGLEVEL(VERBOSE));

struct FormatWithLocation {
    const char* value;
    std::source_location loc;

    FormatWithLocation(const char* s, const std::source_location& l =
                                          std::source_location::current())
        : value(s), loc(l) {}
};

template <typename... Args>
void LOG(LogLevel servere, FormatWithLocation fwl, Args... args) {
    const char* logmsg = nullptr;
    const auto& loc = fwl.loc;
    const auto& fmt = fwl.value;

#ifdef NDEBUG
    if (servere == LogLevel::VERBOSE) {
        return;
    }
#endif
    if constexpr (sizeof...(args) == 0) {
        logmsg = fmt;
    } else {
        static char logbuf[512];
        memset(logbuf, 0, sizeof(logbuf));
        snprintf(logbuf, sizeof(logbuf), fmt, args...);
        logmsg = logbuf;
    }
    printf("[%s:%d] %s: %s\n", loc.file_name(), loc.line(),
           array_helpers::find(LogLevelStrMap, servere)->second, logmsg);
}

#ifndef NDEBUG
#define ABORT_FN(cond) assert(cond)
#else
#define ABORT_FN(cond) abort()
#endif

#define ASSERT(cond, message, ...)                                             \
    do {                                                                       \
        if (!(cond)) {                                                         \
            LOG(LogLevel::FATAL, "Assertion failed: " message, ##__VA_ARGS__); \
            ABORT_FN(cond);                                                    \
        }                                                                      \
    } while (0)

#define ASSERT_UNREACHABLE                                         \
    {                                                              \
        bool is_unreachable = false;                               \
        ASSERT(is_unreachable, "This path should be unreachable"); \
    }
    