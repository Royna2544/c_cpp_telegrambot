#include <absl/log/log.h>
#include <winsock2.h>

#include "SelectorWindows.hpp"

bool SelectSelector::init() {
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    return true;
}

bool SelectSelector::add(socket_handle_t fd, OnSelectedCallback callback,
                         Mode mode) {
    fd_set *set = nullptr;
    fd_set *other_set = nullptr;
    switch (mode) {
        case Mode::READ:
            set = &read_set;
            other_set = &write_set;
            break;
        case Mode::WRITE:
            set = &write_set;
            other_set = &read_set;
            break;
        case Selector::Mode::READ_WRITE:
        case Selector::Mode::EXCEPT:
            break;
    }
    if (FD_ISSET(fd, set) || FD_ISSET(fd, other_set)) {
        LOG(WARNING) << "fd " << fd << " already set";
        return false;
    }
    FD_SET(fd, set);
    data.emplace_back(fd, callback, mode);
    return true;
}

bool SelectSelector::remove(socket_handle_t fd) {
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
    // TODO: Windows only uses timeout
    struct timeval tv {
        .tv_sec = getSOrDefault()
    };

    int ret = select(FD_SETSIZE, &read_set, &write_set, nullptr, &tv);
    if (ret == SOCKET_ERROR) {
        LOG(ERROR) << "Select failed: " << WSAGetLastError();
        return SelectorPollResult::FAILED;
    }
    for (auto &e : data) {
        if (FD_ISSET(e.fd, &read_set) || FD_ISSET(e.fd, &write_set)) {
            e.callback();
        }
    }
    // We have used the select: reinit them
    reinit();
    return SelectorPollResult::OK;
}

void SelectSelector::shutdown() {}

bool SelectSelector::reinit() {
    init();
    for (auto &e : data) {
        switch (e.mode) {
            case Mode::READ:
                FD_SET(e.fd, &read_set);
                break;
            case Mode::WRITE:
                FD_SET(e.fd, &write_set);
                break;
        }
    }
    return true;
}