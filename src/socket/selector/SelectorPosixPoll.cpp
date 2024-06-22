#include <internal/_FileDescriptor_posix.h>
#include <poll.h>
#include <sys/poll.h>

#include <memory>

#include "SelectorPosix.hpp"

bool PollSelector::init() { return true; }

bool PollSelector::add(socket_handle_t fd, OnSelectedCallback callback) {
    struct pollfd pfd {};

    if (std::ranges::any_of(pollfds, [fd](const auto &data) {
            return data.poll_fd.fd == fd;
        })) {
        LOG(WARNING) << "poll fd " << fd << " is already added";
        return false;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pollfds.push_back({pfd, callback});
    return true;
}

bool PollSelector::remove(socket_handle_t fd) {
    bool ret = false;
    std::erase_if(pollfds, [fd, &ret](const PollFdData &e) {
        if (e.poll_fd.fd == fd) {
            ret = true;
            return true;
        }
        return false;
    });
    return ret;
}

PollSelector::SelectorPollResult PollSelector::poll() {
    const size_t fds_len = pollfds.size();
    bool any = false;

    auto pfds = std::make_unique<struct pollfd[]>(fds_len);
    for (size_t i = 0; i < fds_len; ++i) {
        pfds[i] = pollfds[i].poll_fd;
    }
    int rc = ::poll(pfds.get(), pollfds.size(), getMsOrDefault());
    if (rc < 0) {
        PLOG(ERROR) << "Poll failed";
        return SelectorPollResult::FAILED;
    }
#define CHECK_AND_WARN(event, x)         \
    if ((event) & (x)) {                     \
        LOG(WARNING) << #x << " is set"; \
    }
#define NOTHING 0
    
    for (int i = 0; i < pollfds.size(); ++i) {
        const auto revents = pfds[i].revents;
        LOG(INFO) << "My fd: " << pollfds[i].poll_fd.fd;
        if (revents == 0) {
            LOG(INFO) << "Nothing is set";
            continue;
        }
        if (revents & POLLIN) {
            LOG(INFO) << "POLLIN is set";
            pollfds[i].callback();
            any = true;
        }
        CHECK_AND_WARN(revents, POLLPRI);
        CHECK_AND_WARN(revents, POLLERR);
        CHECK_AND_WARN(revents, POLLHUP);
        CHECK_AND_WARN(revents, POLLNVAL);
        CHECK_AND_WARN(revents, POLLOUT);
        pfds[i].revents = 0;
    }
    DLOG(INFO) << "Return value: " << rc
               << ", Callbacks called: " << std::boolalpha << any;
    if (!any) {
        LOG(WARNING) << "None of the fd returned POILLIN";
        return SelectorPollResult::TIMEOUT;
    }
    return SelectorPollResult::OK;
}

void PollSelector::shutdown() {}