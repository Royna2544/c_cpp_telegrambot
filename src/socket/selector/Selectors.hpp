#pragma once

#include <memory>
#include <functional>

#include <SocketDescriptor_defs.hpp>

// Base interface for a fd selector, e.g. poll(2) or select(2).
struct Selector {
    enum class SelectorPollResult {
        OK = 0,
        FAILED = -1,
    };
    static constexpr int kTimeoutSecs = 5;

    using OnSelectedCallback = std::function<void(void)>;

    virtual ~Selector() = default;

    // Initialize the selector, return false on failure.
    virtual bool init() = 0;

    // Re-initialize the selector, return false on failure.
    virtual bool reinit() { return true; }

    // Add a file descriptor to the selector, with a callback. return false on failure.
    virtual bool add(socket_handle_t fd, OnSelectedCallback callback) = 0;

    // Remove a file descriptor from the selector, return false on failure.
    virtual bool remove(socket_handle_t fd) = 0;

    // Poll the selector, return SelectorPollResult object.
    virtual SelectorPollResult poll() = 0;

    // Shutdown the selector.
    virtual void shutdown() = 0;
};
