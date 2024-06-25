#include "ServerBackend.hpp"

#include <ConfigManager.h>

#include <iomanip>

SocketServerWrapper::SocketServerWrapper() {
    auto value =
        ConfigManager::getVariable(ConfigManager::Configs::SOCKET_BACKEND);
    if (!value) {
        LOG(ERROR) << "No socket backend specified, not creating sockets";
        return;
    }
    auto& string = *value;
    auto indexOfSep = string.find(',');
    if (indexOfSep == std::string::npos) {
        LOG(INFO) << "Bootstrap interface: " << std::quoted(string);
        internalBackend = getInterfaceForString(string);
    } else {
        auto first = string.substr(0, indexOfSep);
        auto second = string.substr(indexOfSep + 1);
        if (first == second) {
            LOG(ERROR) << "Two interfaces cannot be the same! (Same as "
                       << std::quoted(first) << ")";
            LOG(ERROR) << "Skip creating socket interfaces";
            return;
        }
        LOG(INFO) << "Bootstrap i:" << first << " e:" << second << " interface";
        internalBackend = getInterfaceForString(first);
        externalBackend = getInterfaceForString(second);
    }
}

SocketServerWrapper::BackendType SocketServerWrapper::fromString(
    const std::string& str) {
    if (str == "ipv4") {
        return BackendType::Ipv4;
    }
    if (str == "ipv6") {
        return BackendType::Ipv6;
    }
    if (str == "local") {
        return BackendType::Local;
    }
    LOG(ERROR) << "Unknown backend type: " << str;
    return BackendType::Unknown;
}

std::shared_ptr<SocketInterfaceBase> SocketServerWrapper::getInterfaceForString(
    const std::string& str) {
    return getInterfaceForType(fromString(str));
}