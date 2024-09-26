// clang-format off
#include <winsock2.h>
#include <winsock.h>
#include <afunix.h>
#include <windows.h>
// clang-format on
#include <boost/algorithm/string/trim.hpp>
#include <impl/SocketWindows.hpp>
#include <socket/selector/SelectorWindows.hpp>

#include "SocketDescriptor_defs.hpp"
#include "helper/HelperWindows.hpp"
#include "socket/selector/Selectors.hpp"

std::string SocketInterfaceWindows::WSALastErrorStr() {
    char *s = nullptr;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr, WSAGetLastError(),
                  MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPSTR)&s, 0,
                  nullptr);
    std::string ret(s);
    boost::trim(ret);
    LocalFree(s);
    return ret;
}

void SocketInterfaceWindows::startListening(
    socket_handle_t handle, const listener_callback_t onNewData) {
    bool should_break = false;
    WSADATA data;
    SelectSelector selector;

    do {
        if (listen(handle, SOMAXCONN) == SOCKET_ERROR) {
            LOG(ERROR) << "Failed to listen to socket: " << WSAEStr();
            break;
        }
        if (!selector.init()) {
            break;
        }
        selector.add(handle, [handle, this, &should_break, onNewData] {
            struct sockaddr addr {};
            socklen_t len = sizeof(addr);
            const socket_handle_t cfd = accept(handle, &addr, &len);

            if (!isValidSocketHandle(cfd)) {
                LOG(ERROR) << "Failed to accept: " << WSAEStr();
            } else {
                printRemoteAddress(cfd);
                SocketConnContext ctx(cfd, addr);
                should_break = onNewData(ctx);
                closesocket(cfd);
            }
        }, Selector::Mode::READ);
        while (!should_break && kRun) {
            switch (selector.poll()) {
                case Selector::SelectorPollResult::FAILED:
                    LOG(ERROR) << "Poll failed";
                    should_break = true;
                    break;
                case Selector::SelectorPollResult::OK:
                    break;
            }
        }
        selector.shutdown();
    } while (false);
    closeSocketHandle(handle);
    cleanupServerSocket();
}

bool SocketInterfaceWindows::writeToSocket(SocketConnContext context,
                                           SharedMalloc data) {
    auto *socketData = static_cast<char *>(data.get());
    auto *addr = static_cast<sockaddr *>(context.addr.get());
    int count = 0;
    if (useUdp(this)) {
        count = sendto(context.cfd, socketData, data->size(), 0, addr,
                       context.addr->size());

    } else {
        count = send(context.cfd, socketData, data->size(), 0);
    }
    if (count < 0) {
        LOG(ERROR) << "Failed to sent to socket: " << WSAEStr();
        return false;
    }
    return true;
}

void SocketInterfaceWindows::forceStopListening(void) { kRun = false; }

std::optional<SharedMalloc> SocketInterfaceWindows::readFromSocket(
    SocketConnContext context, TgBotSocket::PacketHeader::length_type length) {
    SharedMalloc buf(length);
    auto *addr = static_cast<sockaddr *>(context.addr.get());
    auto *data = static_cast<char *>(buf.get());
    socklen_t addrLen = context.addr->size();
    int count = 0;

    if (useUdp(this)) {
        count = recvfrom(context.cfd, data, length, 0, addr, &addrLen);
    } else {
        count = recv(context.cfd, data, length, 0);
    }
    if (count != length) {
        LOG(ERROR) << "Failed to read from socket: " << WSAEStr();
    } else {
        return buf;
    }
    return std::nullopt;
}

bool SocketInterfaceWindows::closeSocketHandle(socket_handle_t &handle) {
    if (isValidSocketHandle(handle)) {
        closesocket(handle);
        handle = INVALID_SOCKET;
        return true;
    }
    return false;
}

bool SocketInterfaceWindows::setSocketOptTimeout(socket_handle_t handle,
                                                 int timeout) {
    DWORD timeoutD = timeout;
    int ret = 0;

    ret |= setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeoutD,
                      sizeof(timeoutD));
    ret |= setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeoutD,
                      sizeof(timeoutD));
    if (ret) {
        LOG(ERROR) << "Failed to set socket timeout: " << WSAEStr();
    }
    return !ret;
}