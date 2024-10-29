#include "ServerBackend.hpp"
#include "impl/SocketPosix.hpp"

std::shared_ptr<SocketInterfaceBase> SocketServerWrapper::getInterfaceForType(
    const BackendType type) {
    switch (type) {
        case BackendType::Ipv4:
            return std::make_unique<SocketInterfaceUnixIPv4>();
        case BackendType::Ipv6:
            return std::make_unique<SocketInterfaceUnixIPv6>();
        case BackendType::Local:
            return std::make_unique<SocketInterfaceUnixLocal>();
        case BackendType::Unknown:
            break;
    }
    return nullptr;
}