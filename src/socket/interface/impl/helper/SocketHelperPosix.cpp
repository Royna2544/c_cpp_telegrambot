#include <impl/SocketPosix.hpp>

#include "HelperPosix.hpp"

int SocketInterfaceUnix::PosixHelper::getSocketType() {
    if (static_cast<bool>(interface->options.use_udp) &&
        interface->options.use_udp.get()) {
        return SOCK_DGRAM;
    }
    return SOCK_STREAM;
}

bool SocketInterfaceUnix::PosixHelper::connectionTimeoutEnabled() {
    return static_cast<bool>(interface->options.use_connect_timeout) &&
           interface->options.use_connect_timeout.get();
}

void SocketInterfaceUnix::PosixHelper::handleConnectTimeoutPre(
    socket_handle_t socket) {
    setSocketFlags<O_NONBLOCK, true>(socket);
}

bool SocketInterfaceUnix::PosixHelper::handleConnectTimeoutPost(
    socket_handle_t socket) {
    UnixSelector selector;

    LOG(INFO) << "Connecting timeout mode enabled";
    selector.add(socket, []() {}, Selector::Mode::WRITE);
    selector.enableTimeout(true);
    selector.setTimeout(interface->options.connect_timeout.get());
    switch (selector.poll()) {
        case Selector::PollResult::OK:
            break;
        case Selector::PollResult::FAILED:
        case Selector::PollResult::TIMEOUT:
            LOG(ERROR) << "Connecting timeout";
            return false;
    }
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
        PLOG(ERROR) << "getsockopt(SO_ERROR) failed";
        return false;
    }

    if (error != 0) {
        LOG(ERROR) << "Failed to connect: " << strerror(error);
        return false;
    }

    LOG(INFO) << "Connected";
    setSocketFlags<O_NONBLOCK, false>(socket);
    return true;
}