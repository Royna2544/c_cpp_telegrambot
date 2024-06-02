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
    int rc = ::poll(pfds.get(), pollfds.size(), -1);
    if (rc < 0) {
        PLOG(ERROR) << "Poll failed";
        return SelectorPollResult::FAILED;
    }
    for (int i = 0; i < pollfds.size(); ++i) {
        if (pfds[i].revents & POLLIN) {
            pollfds[i].callback();
            pfds[i].revents = 0;
            any = true;
        }
    }
    if (!any) {
        LOG(WARNING) << "None of the fd returned POILLIN";
    }
    return SelectorPollResult::OK;
}

void PollSelector::shutdown() {}