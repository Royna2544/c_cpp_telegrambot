#include "ServerBackend.hpp"
#include "impl/SocketWindows.hpp"

std::shared_ptr<SocketInterfaceBase> SocketServerWrapper::getInterfaceForType(
    const BackendType type) {
    switch (type) {
        case BackendType::Ipv4:
            return std::make_shared<SocketInterfaceWindowsIPv4>();
        case BackendType::Ipv6:
            return std::make_shared<SocketInterfaceWindowsIPv6>();
        case BackendType::Local:
            return std::make_shared<SocketInterfaceWindowsLocal>();
        case BackendType::Unknown:
            break;
    }
    return nullptr;
}