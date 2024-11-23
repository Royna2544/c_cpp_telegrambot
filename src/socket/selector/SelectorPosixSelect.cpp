#include <absl/log/log.h>
#include <sys/select.h>
#include <algorithm>

#include "SelectorPosix.hpp"
#include "socket/selector/Selectors.hpp"

bool SelectSelector::init() {
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    FD_ZERO(&except_set);
    return true;
}

bool SelectSelector::add(Selector::HandleType fd, OnSelectedCallback callback,
                         Mode mode) {
    std::array<fd_set *, 3> sets{};
    std::array<fd_set *, 3> other_sets{};

    switch (mode) {
        case Mode::READ:
            sets = {&read_set};
            other_sets = {&write_set, &except_set};
            break;
        case Mode::WRITE:
            sets = {&write_set};
            other_sets = {&read_set, &except_set};
            break;
        case Mode::READ_WRITE:
            sets = {&read_set, &write_set};
            other_sets = {&except_set};
            break;
        case Mode::EXCEPT:
            sets = {&except_set};
            other_sets = {&read_set, &write_set};
            break;
    }
    bool existsHere = std::ranges::any_of(sets, [fd](const fd_set* set) {
        return set != nullptr && FD_ISSET(fd, set);
    });
    bool existsThere = std::ranges::any_of(other_sets, [fd](const fd_set* set) {
        return set != nullptr && FD_ISSET(fd, set);
    });
    if (existsHere || existsThere) {
        LOG(WARNING) << "fd " << fd << " already set";
        return false;
    }
    std::ranges::for_each(sets, [fd](fd_set* set) {
        if (set != nullptr) {
            FD_SET(fd, set);
        }
    });
    data.emplace_back(fd, callback, mode);
    return true;
}

bool SelectSelector::remove(Selector::HandleType fd) {
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

SelectSelector::PollResult SelectSelector::poll() {
    struct timeval timeout {
        .tv_sec = getSOrDefault()
    };
    bool any = false;
    int ret = select(FD_SETSIZE, &read_set, &write_set, nullptr,
                     isTimeoutEnabled() ? &timeout : nullptr);
    if (ret < 0) {
        PLOG(ERROR) << "Select failed";
        return PollResult::FAILED;
    }
    for (auto &e : data) {
        if (FD_ISSET(e.fd, &read_set) || FD_ISSET(e.fd, &write_set)) {
            e.callback();
            any = true;
        }
    }
    if (!any) {
        LOG(WARNING) << "No events";
        return PollResult::TIMEOUT;
    }
    // We have used the select: reinit them
    reinit();
    return PollResult::OK;
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
            case Selector::Mode::READ_WRITE:
                FD_SET(e.fd, &read_set);
                FD_SET(e.fd, &write_set);
                break;
            case Selector::Mode::EXCEPT:
                FD_SET(e.fd, &except_set);
                break;
        }
    }
    return true;
}