
#include "SelectorPosix.hpp"
#include "SocketDescriptor_defs.hpp"

bool EPollSelector::init() { return false; }

bool EPollSelector::add(socket_handle_t /*fd*/, OnSelectedCallback /*callback*/,
                        Mode /*mode*/) {
    return false;
}

bool EPollSelector::remove(socket_handle_t /*fd*/) { return false; }

EPollSelector::SelectorPollResult EPollSelector::poll() {
    return SelectorPollResult::FAILED;
}

void EPollSelector::shutdown() {}

bool EPollSelector::reinit() { return false; }