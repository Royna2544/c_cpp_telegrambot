
#include <sys/poll.h>
#include <sys/select.h>

#include <memory>
#include <utility>
#include <variant>

#include "Selectors.hpp"

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

struct SelectSelector : Selector {
    bool init() override;
    bool add(HandleType fd, OnSelectedCallback callback, Mode mode) override;
    bool remove(HandleType fd) override;
    PollResult poll() override;
    void shutdown() override;
    bool reinit() override;

   private:
    struct SelectFdData {
        HandleType fd;
        OnSelectedCallback callback;
        Mode mode;

        explicit SelectFdData(HandleType fd, OnSelectedCallback callback,
                              Mode mode)
            : fd(fd), callback(std::move(callback)), mode(mode) {}
    };
    fd_set read_set;
    fd_set write_set;
    fd_set except_set;
    std::vector<SelectFdData> data;
};

struct EPollSelector : Selector {
    bool init() override;
    bool add(HandleType fd, OnSelectedCallback callback, Mode mode) override;
    bool remove(HandleType fd) override;
    PollResult poll() override;
    void shutdown() override;
    bool reinit() override;

   private:
    static constexpr int MAX_EPOLLFDS = 16;
    struct EPollFdData {
        int fd;
        OnSelectedCallback callback;
    };
    int epollfd;
    std::vector<EPollFdData> data;
};

// Wrapper around selectors
struct UnixSelector {
    explicit UnixSelector();

    bool init();
    bool add(Selector::HandleType fd, Selector::OnSelectedCallback callback,
             Selector::Mode flags);
    bool remove(Selector::HandleType fd);
    Selector::PollResult poll();
    void shutdown();
    bool reinit();
    void enableTimeout(bool enabled);

    // Set the timeout for the selector.
    template <typename Rep, typename Period>
    void setTimeout(const std::chrono::duration<Rep, Period> timeout) {
        if (m_selector) {
            m_selector->setTimeout(timeout);
        }
    }

   private:
    std::unique_ptr<Selector> m_selector;
};
