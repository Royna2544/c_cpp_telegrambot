#include <impl/SocketWindows.hpp>
#include "HelperWindows.hpp"

bool SocketInterfaceWindows::WinHelper::createInetSocketAddr(
    SocketConnContext& context) {
    context.cfd = socket(AF_INET, getSocketType(_interface), 0);
    if (!_interface->isValidSocketHandle(context.cfd)) {
        LOG(ERROR) << "Failed to create socket: " << WSALastErrorStr();
        return false;
    }
    auto* addr = static_cast<sockaddr_in*>(context.addr.get());

    addr->sin_family = AF_INET;
    addr->sin_port = htons(_interface->helper.inet.getPortNum());
    return true;
}

bool SocketInterfaceWindows::WinHelper::createInet6SocketAddr(
    SocketConnContext& context) {
    context.cfd = socket(AF_INET6, getSocketType(_interface), 0);
    if (!_interface->isValidSocketHandle(context.cfd)) {
        LOG(ERROR) << "Failed to create socket: " << WSALastErrorStr();
        return false;
    }

    auto* addr = static_cast<sockaddr_in6*>(context.addr.get());
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(_interface->helper.inet.getPortNum());
    return true;
};