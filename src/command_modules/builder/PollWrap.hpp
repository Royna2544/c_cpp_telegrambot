#pragma once

#include <absl/log/log.h>

#include <chrono>
#include <functional>
#include <optional>

#include <poll.h>
#include <sys/poll.h>

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

    using OnSelectedCallback = std::function<void(void)>;
#ifdef _WIN32
    using HandleType = SOCKET;
#else
    using HandleType = int;
#endif

    static constexpr std::chrono::seconds kDefaultTimeoutSecs{5};

    virtual ~Selector() = default;

    // Initialize the selector, return false on failure.
    virtual bool init() = 0;

    // Re-initialize the selector, return false on failure.
    virtual bool reinit() { return true; }

    // Add a file descriptor to the selector, with a callback. return false on
    // failure.
    virtual bool add(HandleType fd, OnSelectedCallback callback, Mode mode) = 0;

    // Remove a file descriptor from the selector, return false on failure.
    virtual bool remove(HandleType fd) = 0;

    // Poll the selector, return PollResult object.
    virtual PollResult poll() = 0;

    // Shutdown the selector.
    virtual void shutdown() = 0;
};

struct PollSelector : Selector {
    bool init() override;
    bool add(HandleType fd, OnSelectedCallback callback, Mode mode) override;
    bool remove(HandleType fd) override;
    PollResult poll() override;
    void shutdown() override;

   private:
    struct PollFdData {
        struct pollfd poll_fd;
        OnSelectedCallback callback;

        explicit PollFdData(pollfd poll_fd, OnSelectedCallback callback)
            : poll_fd(poll_fd), callback(std::move(callback)) {}
    };
    std::vector<PollFdData> pollfds;
};
