#include "SocketInterfaceGetter.hpp"
#include "../SocketInterfaceUnix.h"

std::shared_ptr<SocketInterfaceBase> SocketInterfaceGetter::getForClient() {
    static const std::vector<std::shared_ptr<SocketInterfaceBase>>
        socket_interfaces_client{
            std::make_shared<SocketInterfaceUnixIPv4>(),
            std::make_shared<SocketInterfaceUnixIPv6>(),
            std::make_shared<SocketInterfaceUnixLocal>(),
        };

    for (const auto& e : socket_interfaces_client) {
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
    auto& mgr = SingleThreadCtrlManager::getInstance();
    switch (type) {
        case SocketNetworkType::TYPE_IPV4:
            ptr = mgr.getController<SocketInterfaceUnixIPv4>(tusage);
        case SocketNetworkType::TYPE_IPV6:
            ptr = mgr.getController<SocketInterfaceUnixIPv6>(tusage);
        case SocketNetworkType::TYPE_LOCAL_UNIX:
            ptr = mgr.getController<SocketInterfaceUnixLocal>(tusage);
    };
    return ptr;
}