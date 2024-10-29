#include "ServerBackend.hpp"

#include <ConfigManager.hpp>

#include <iomanip>

SocketServerWrapper::SocketServerWrapper(std::string config) {
    auto indexOfSep = config.find(',');
    if (indexOfSep == std::string::npos) {
        LOG(INFO) << "Bootstrap interface: " << std::quoted(config);
        internal = fromString(config);
    } else {
        auto first = config.substr(0, indexOfSep);
        auto second = config.substr(indexOfSep + 1);
        if (first == second) {
            LOG(ERROR) << "Two interfaces cannot be the same! (Same as "
                       << std::quoted(first) << ")";
            LOG(ERROR) << "Skip creating socket interfaces";
            return;
        }
        LOG(INFO) << "Bootstrap i:" << first << " e:" << second << " interface";
        internal = fromString(first);
        internal = fromString(second);
    }
}

SocketServerWrapper::BackendType SocketServerWrapper::fromString(
    const std::string_view str) {
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
