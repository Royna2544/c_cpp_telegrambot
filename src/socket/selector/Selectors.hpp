#pragma once

#include <absl/log/log.h>

#include <chrono>
#include <functional>
#include <optional>

#include "../include/SocketDescriptor_defs.hpp"
#include "internal/_std_chrono_templates.h"

// Base interface for a fd selector, e.g. poll(2) or select(2).
struct Selector {
    enum class PollResult {
        OK = 0,
        FAILED = -1,
        TIMEOUT = -2,
    };
    enum class Mode {
        READ,        // When reading would not block
        WRITE,       // When writing would not block
        READ_WRITE,  // When either reading or writing would not block
        EXCEPT,      // When exception happens
    };

    // Shim for old code
    using SelectorPollResult = PollResult;
    using OnSelectedCallback = std::function<void(void)>;
    static constexpr std::chrono::seconds kDefaultTimeoutSecs{5};

    virtual ~Selector() = default;

    // Initialize the selector, return false on failure.
    virtual bool init() = 0;

    // Re-initialize the selector, return false on failure.
    virtual bool reinit() { return true; }

    // Add a file descriptor to the selector, with a callback. return false on
    // failure.
    virtual bool add(socket_handle_t fd, OnSelectedCallback callback,
                     Mode mode) = 0;

    // Remove a file descriptor from the selector, return false on failure.
    virtual bool remove(socket_handle_t fd) = 0;

    // Poll the selector, return SelectorPollResult object.
    virtual SelectorPollResult poll() = 0;

    // Shutdown the selector.
    virtual void shutdown() = 0;

    // Check if timeout feature is available.
    [[nodiscard]] virtual bool isTimeoutAvailable() const { return true; }

    // Enable/disable timeout for selector
    void enableTimeout(bool enabled) {
        if (!isTimeoutAvailable()) {
            LOG(WARNING) << "Timeout is not available for selector";
            return;
        }
        // If we are enabling timeout, set it to the configured value
        if (enabled && !timeoutMillisec) {
            timeoutMillisec = to_msecs(timeoutConfig);
        }
        // If we are disabling timeout, reset the optional
        if (!enabled && timeoutMillisec) {
            timeoutMillisec = std::nullopt;
        }
    }

    // Check if timeout is enabled.
    [[nodiscard]] bool isTimeoutEnabled() const {
        return timeoutMillisec.has_value();
    }

    // Set the timeout for the selector.
    template <typename Rep, typename Period>
    void setTimeout(const std::chrono::duration<Rep, Period> timeout) {
        if (!isTimeoutAvailable()) {
            LOG(WARNING) << "Timeout is not available for selector";
            return;
        }
        timeoutConfig = to_secs(timeout);
        if (timeoutMillisec) {
            timeoutMillisec = to_msecs(timeout);
        }
    }

   protected:
    int getMsOrDefault(int defaultValue = -1) {
        if (timeoutMillisec) {
            return timeoutMillisec->count();
        }
        return defaultValue;
    }
    int getSOrDefault(int defaultValue = -1) {
        if (timeoutMillisec) {
            return to_secs(timeoutMillisec.value()).count();
        }
        return defaultValue;
    }

   private:
    // Actual value used in sub selectors
    std::optional<std::chrono::milliseconds> timeoutMillisec;
    // Configured value, not directly used
    std::chrono::seconds timeoutConfig = kDefaultTimeoutSecs;
};
