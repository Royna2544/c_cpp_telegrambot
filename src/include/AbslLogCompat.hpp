#pragma once

/**
 * Compatibility layer for transitioning from Abseil logging to spdlog
 * This header provides macros that map Abseil logging calls to spdlog equivalents
 * Supports both stream-style (<<) and format-style logging
 */

#include <spdlog/spdlog.h>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <atomic>

// Helper class to support stream-style logging with spdlog
namespace detail {
    template<typename Level>
    class LogStream {
        std::ostringstream oss;
        Level level;
        
    public:
        explicit LogStream(Level l) : level(l) {}
        
        template<typename T>
        LogStream& operator<<(const T& value) {
            oss << value;
            return *this;
        }
        
        // Special handling for manipulators like std::endl
        LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
            manip(oss);
            return *this;
        }
        
        ~LogStream() {
            const std::string msg = oss.str();
            if (!msg.empty()) {
                switch(level) {
                    case spdlog::level::info:
                        spdlog::info("{}", msg);
                        break;
                    case spdlog::level::warn:
                        spdlog::warn("{}", msg);
                        break;
                    case spdlog::level::err:
                        spdlog::error("{}", msg);
                        break;
                    case spdlog::level::critical:
                        spdlog::critical("{}", msg);
                        break;
                    case spdlog::level::debug:
                        spdlog::debug("{}", msg);
                        break;
                    default:
                        spdlog::info("{}", msg);
                }
            }
        }
    };
    
    // Helper for LOG_FIRST_N
    template<typename Level>
    class LogFirstNStream {
        std::ostringstream oss;
        Level level;
        std::atomic<int>* counter;
        int n;
        bool should_log;
        
    public:
        LogFirstNStream(Level l, std::atomic<int>* cnt, int n_val) 
            : level(l), counter(cnt), n(n_val) {
            int current = counter->fetch_add(1, std::memory_order_relaxed);
            should_log = (current < n);
        }
        
        template<typename T>
        LogFirstNStream& operator<<(const T& value) {
            if (should_log) {
                oss << value;
            }
            return *this;
        }
        
        LogFirstNStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
            if (should_log) {
                manip(oss);
            }
            return *this;
        }
        
        ~LogFirstNStream() {
            if (should_log) {
                const std::string msg = oss.str();
                if (!msg.empty()) {
                    switch(level) {
                        case spdlog::level::info:
                            spdlog::info("{}", msg);
                            break;
                        case spdlog::level::warn:
                            spdlog::warn("{}", msg);
                            break;
                        case spdlog::level::err:
                            spdlog::error("{}", msg);
                            break;
                        case spdlog::level::critical:
                            spdlog::critical("{}", msg);
                            break;
                        case spdlog::level::debug:
                            spdlog::debug("{}", msg);
                            break;
                        default:
                            spdlog::info("{}", msg);
                    }
                }
            }
        }
    };
}

// Map LOG macros to support stream-style logging
#define LOG(severity) ::detail::LogStream<spdlog::level::level_enum>(spdlog::level::severity)

// Map DLOG (debug log) macros
#ifdef NDEBUG
  #define DLOG(severity) if (false) ::detail::LogStream<spdlog::level::level_enum>(spdlog::level::severity)
#else
  #define DLOG(severity) ::detail::LogStream<spdlog::level::level_enum>(spdlog::level::severity)
#endif

// Map PLOG (log with errno) - logs the error and then continues with stream
#define PLOG(severity) ::detail::LogStream<spdlog::level::level_enum>(spdlog::level::severity) << std::strerror(errno) << ": "

// Map LOG_FIRST_N macro
#define LOG_FIRST_N(severity, n) \
  static std::atomic<int> LOG_FIRST_N_COUNTER_##__LINE__{0}; \
  ::detail::LogFirstNStream<spdlog::level::level_enum>(spdlog::level::severity, &LOG_FIRST_N_COUNTER_##__LINE__, n)

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

// Map severity levels - these are used as parameters to LOG() macro
#define INFO info
#define WARNING warn
#define ERROR err
#define FATAL critical

