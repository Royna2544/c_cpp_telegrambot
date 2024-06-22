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
        internalBackend = getInterfaceForName(string);
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
        internalBackend = getInterfaceForName(first);
        externalBackend = getInterfaceForName(second);
    }
}
