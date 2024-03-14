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

static inline constexpr auto LogLevelStrMap =
    array_helpers::make<static_cast<int>(LogLevel::MAX), LogLevel, const char*>(
        ENUM_AND_STR(LogLevel::FATAL), ENUM_AND_STR(LogLevel::ERROR),
        ENUM_AND_STR(LogLevel::WARNING), ENUM_AND_STR(LogLevel::INFO),
        ENUM_AND_STR(LogLevel::DEBUG), ENUM_AND_STR(LogLevel::VERBOSE));

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

template <typename T, typename... Args>
void ASSERT(const T cond, FormatWithLocation value, Args... args) {
    if (!cond) {
        LOG(LogLevel::FATAL, FormatWithLocation("Assertion failed: %s"), value, args...);
        assert(cond);
    }
}