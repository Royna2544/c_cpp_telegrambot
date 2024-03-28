#include "SocketInterfaceGetter.hpp"
#include "../SocketInterfaceWindows.h"

std::shared_ptr<SocketInterfaceBase> SocketInterfaceGetter::getForClient() {
    static const std::vector<std::shared_ptr<SocketInterfaceBase>>
        socket_interfaces_client{
            std::make_shared<SocketInterfaceWindowsIPv4>(),
            std::make_shared<SocketInterfaceWindowsIPv6>(),
            std::make_shared<SocketInterfaceWindowsLocal>(),
        };

    for (const auto &e : socket_interfaces_client) {
        if (e->isAvailable()) {
            return e;
        }
    }
    return nullptr;
}

std::shared_ptr<SocketInterfaceBase> SocketInterfaceGetter::get(
    const SocketNetworkType type, const SocketUsage usage) {
    const auto tusage =
        static_cast<SingleThreadCtrlManager::ThreadUsage>(usage);
    std::shared_ptr<SocketInterfaceBase> ptr;
    auto &mgr = SingleThreadCtrlManager::getInstance();
    switch (type) {
        case SocketNetworkType::TYPE_IPV4:
            ptr = mgr.getController<SocketInterfaceWindowsIPv4>(tusage);
        case SocketNetworkType::TYPE_IPV6:
            ptr = mgr.getController<SocketInterfaceWindowsIPv6>(tusage);
        case SocketNetworkType::TYPE_LOCAL_UNIX:
            ptr = mgr.getController<SocketInterfaceWindowsLocal>(tusage);
    };
    return ptr;
}