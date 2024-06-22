#include "ServerBackend.hpp"
#include "impl/SocketPosix.hpp"

std::shared_ptr<SocketInterfaceBase> SocketServerWrapper::getInterfaceForName(
    const std::string& name) {
    if (name == "ipv4") {
        return std::make_shared<SocketInterfaceUnixIPv4>();
    }
    if (name == "ipv6") {
        return std::make_shared<SocketInterfaceUnixIPv6>();
    }
    if (name == "local") {
        return std::make_shared<SocketInterfaceUnixLocal>();
    }
    LOG(ERROR) << "Unknown socket backend " << name;
    return nullptr;
}