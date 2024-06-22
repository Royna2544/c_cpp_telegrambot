
#include <sys/poll.h>
#include <sys/select.h>

#include <variant>

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

// Wrapper around selectors
struct UnixSelector {
    explicit UnixSelector();

    bool init();
    bool add(socket_handle_t fd, Selector::OnSelectedCallback callback);
    bool remove(socket_handle_t fd);
    Selector::SelectorPollResult poll();
    void shutdown();
    bool reinit();

   private:
    template <typename T>
    constexpr static bool isKnownSelector() {
        return std::is_same_v<T, PollSelector> ||
               std::is_same_v<T, SelectSelector> ||
               std::is_same_v<T, EPollSelector>;
    }
    std::variant<PollSelector, SelectSelector, EPollSelector> m_selector;
};