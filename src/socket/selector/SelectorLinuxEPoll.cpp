#include <trivial_helpers/_FileDescriptor_posix.h>
#include <sys/epoll.h>

#include <algorithm>
#include <cerrno>

#include "SelectorPosix.hpp"

bool EPollSelector::init() {
    return (isValidFd(epollfd = epoll_create(MAX_EPOLLFDS)));
}

bool EPollSelector::add(socket_handle_t fd, OnSelectedCallback callback,
                        Mode mode) {
    struct epoll_event event {};

    if (data.size() > MAX_EPOLLFDS) {
        LOG(WARNING) << "epoll fd buffer full";
        return false;
    }

    if (std::ranges::any_of(data,
                            [fd](const auto &data) { return data.fd == fd; })) {
        LOG(WARNING) << "epoll fd " << fd << " is already added";
        return false;
    }

    event.data.fd = fd;

    switch (mode) {
        case Mode::READ:
            event.events = EPOLLIN;
            break;
        case Mode::WRITE:
            event.events = EPOLLOUT;
            break;
        case Mode::READ_WRITE:
            event.events = EPOLLIN | EPOLLOUT;
            break;
        case Mode::EXCEPT:
            event.events = EPOLLPRI | EPOLLRDHUP;
            break;
        default:
            LOG(ERROR) << "Invalid mode for socket " << fd;
            return false;
    }
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) < 0) {
        PLOG(ERROR) << "epoll_ctl failed";
        return false;
    }
    data.emplace_back(fd, callback);
    return true;
}

bool EPollSelector::remove(socket_handle_t fd) {
    bool found = std::ranges::any_of(
        data, [fd](const auto &data) { return data.fd == fd; });
    if (found) {
        auto [first, last] = std::ranges::remove_if(
            data, [fd](const auto &data) { return data.fd == fd; });
        data.erase(first, last);
        if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
            PLOG(ERROR) << "epoll_ctl failed";
            return false;
        }
    } else {
        LOG(WARNING) << "epoll fd " << fd << " is not added";
        return false;
    }
    return found;
}

#define CALL_RETRY(retvar, expression) \
    do {                               \
        retvar = (expression);         \
    } while (retvar == -1 && errno == EINTR);

EPollSelector::SelectorPollResult EPollSelector::poll() {
    epoll_event result_event{};
    int result = 0;
    CALL_RETRY(result, epoll_wait(epollfd, &result_event, 1, getSOrDefault()));
    if (result < 0) {
        PLOG(ERROR) << "epoll_wait failed";
        return SelectorPollResult::FAILED;
    } else if (result > 0) {
        bool any = false;
        for (const auto &data : data) {
            if (data.fd == result_event.data.fd) {
                data.callback();
                any = true;
                break;
            }
        }
        if (!any) {
            LOG(WARNING) << "Nothing reported";
        }
        return SelectorPollResult::TIMEOUT;
    }
    return SelectorPollResult::OK;
}

void EPollSelector::shutdown() {
    if (isValidFd(epollfd)) {
        ::closeFd(epollfd);
    }
}

bool EPollSelector::reinit() { return true; }