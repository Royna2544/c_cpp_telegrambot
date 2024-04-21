#pragma once

#include <memory>
#include <functional>

#include <SocketDescriptor_defs.hpp>

#undef ERROR_TIMEOUT

// Base interface for a fd selector, e.g. poll(2) or select(2).
struct Selector {
    enum class SelectorPollResult {
        OK = 0,
        ERROR_GENERIC = -1,
        ERROR_TIMEOUT = -2,
        ERROR_NOTHING_FOUND = -3,
    };
    using OnSelectedCallback = std::function<void(void)>;

    virtual ~Selector() = default;

    // Initialize the selector, return false on failure.
    virtual bool init() = 0;

    // Add a file descriptor to the selector, with a callback. return false on failure.
    virtual bool add(socket_handle_t fd, OnSelectedCallback callback) = 0;

    // Remove a file descriptor from the selector, return false on failure.
    virtual bool remove(socket_handle_t fd) = 0;

    // Poll the selector, return SelectorPollResult object.
    virtual SelectorPollResult poll() = 0;

    // Shutdown the selector.
    virtual void shutdown() = 0;
};