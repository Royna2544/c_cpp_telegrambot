#include <absl/log/log.h>
#include <sys/select.h>

#include "SelectorPosix.hpp"

bool SelectSelector::init() {
    FD_ZERO(&set);
    return true;
}

bool SelectSelector::add(int fd, OnSelectedCallback callback) {
    if (FD_ISSET(fd, &set)) {
        LOG(WARNING) << "fd " << fd << " already set";
        return false;
    }
    FD_SET(fd, &set);
    data.push_back({fd, callback});
    return true;
}

bool SelectSelector::remove(int fd) {
    bool ret = false;
    std::erase_if(data, [fd, &ret](const SelectFdData &e) {
        if (e.fd == fd) {
            ret = true;
            return true;
        }
        return false;
    });
    return ret;
}

SelectSelector::SelectorPollResult SelectSelector::poll() {
    struct timeval tv {
        .tv_sec = 5
    };
    bool any = false;

    int ret = select(FD_SETSIZE, &set, nullptr, nullptr, &tv);
    if (ret < 0) {
        PLOG(ERROR) << "Select failed";
        return SelectorPollResult::ERROR_GENERIC;
    }
    for (auto &e : data) {
        if (FD_ISSET(e.fd, &set)) {
            e.callback();
            any = true;
        }
    }
    if (!any) {
        return SelectorPollResult::ERROR_NOTHING_FOUND;
    } else {
        return SelectorPollResult::OK;
    }
}

void SelectSelector::shutdown() {}