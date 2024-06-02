
#include <sys/poll.h>
#include <sys/select.h>

#include "Selectors.hpp"

struct PollSelector : Selector {
    bool init() override;
    bool add(socket_handle_t fd, OnSelectedCallback callback) override;
    bool remove(socket_handle_t fd) override;
    SelectorPollResult poll() override;
    void shutdown() override;

   private:
    struct PollFdData {
        struct pollfd poll_fd;
        OnSelectedCallback callback;
    };
    std::vector<PollFdData> pollfds;
};

struct SelectSelector : Selector {
    bool init() override;
    bool add(socket_handle_t fd, OnSelectedCallback callback) override;
    bool remove(socket_handle_t fd) override;
    SelectorPollResult poll() override;
    void shutdown() override;
    bool reinit() override;

   private:
    struct SelectFdData {
        int fd;
        OnSelectedCallback callback;
    };
    fd_set set;
    std::vector<SelectFdData> data;
};

struct EPollSelector : Selector {
    bool init() override;
    bool add(socket_handle_t fd, OnSelectedCallback callback) override;
    bool remove(socket_handle_t fd) override;
    SelectorPollResult poll() override;
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
