#include "ServerBackend.hpp"
#include "impl/SocketWindows.hpp"

std::shared_ptr<SocketInterfaceBase> SocketServerWrapper::getInterfaceForName(
    const std::string& name) {
    if (name == "ipv4") {
        return std::make_shared<SocketInterfaceWindowsIPv4>();
    }
    if (name == "ipv6") {
        return std::make_shared<SocketInterfaceWindowsIPv6>();
    }
    if (name == "local") {
        return std::make_shared<SocketInterfaceWindowsLocal>();
    }
    LOG(ERROR) << "Unknown socket backend " << name;
    return nullptr;
}