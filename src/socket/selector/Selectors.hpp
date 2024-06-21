#pragma once

#include <SocketDescriptor_defs.hpp>
#include <functional>
#include <optional>
#include <absl/log/log.h>

// Base interface for a fd selector, e.g. poll(2) or select(2).
struct Selector {
    enum class SelectorPollResult {
        OK = 0,
        FAILED = -1,
        TIMEOUT = -2,
    };
    static constexpr int kDefaultTimeoutSecs = 5;

    using OnSelectedCallback = std::function<void(void)>;

    virtual ~Selector() = default;

    // Initialize the selector, return false on failure.
    virtual bool init() = 0;

    // Re-initialize the selector, return false on failure.
    virtual bool reinit() { return true; }

    // Add a file descriptor to the selector, with a callback. return false on
    // failure.
    virtual bool add(socket_handle_t fd, OnSelectedCallback callback) = 0;

    // Remove a file descriptor from the selector, return false on failure.
    virtual bool remove(socket_handle_t fd) = 0;

    // Poll the selector, return SelectorPollResult object.
    virtual SelectorPollResult poll() = 0;

    // Shutdown the selector.
    virtual void shutdown() = 0;

    // Check if timeout feature is available.
    [[nodiscard]] virtual bool isTimeoutAvailable() const {
        return true;
    }

    // Enable/disable timeout for selector
    void enableTimeout(bool enabled) {
        if (!isTimeoutAvailable()) {
            LOG(WARNING) << "Timeout is not available for selector";
            return;
        }
        // If we are enabling timeout, set it to the configured value
        if (enabled && !timeoutSec) {
            timeoutSec = timeoutConfig;
        }
        // If we are disabling timeout, reset the optional
        if (!enabled && timeoutSec) {
            timeoutSec = std::nullopt;
        }
    }

    // Set the timeout for the selector.
    void setTimeout(const int timeout) {
        if (!isTimeoutAvailable()) {
            LOG(WARNING) << "Timeout is not available for selector";
            return;
        }
        timeoutConfig = timeout;
        if (timeoutSec) {
            timeoutSec = timeout;
        }
    }

   protected:
    // Actual value used in sub selectors
    std::optional<int> timeoutSec;

   private:
    // Configured value, not directly used
    int timeoutConfig = kDefaultTimeoutSecs;
};
