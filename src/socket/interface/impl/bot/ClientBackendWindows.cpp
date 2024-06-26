#include <ConfigManager.h>

#include <TryParseStr.hpp>
#include <string>

#include "ClientBackend.hpp"
#include "impl/SocketWindows.hpp"

SocketClientWrapper::SocketClientWrapper() {
    std::string addressString;
    int port = 0;
    bool needPortCfg = false;
    bool foundPort = false;
    if (ConfigManager::getEnv(kIPv4EnvVar.data(), addressString)) {
        backend = std::make_shared<SocketInterfaceWindowsIPv4>();
        needPortCfg = true;
        LOG(INFO) << "Chose IPv4 with address " << addressString;
    } else if (ConfigManager::getEnv(kIPv6EnvVar.data(), addressString)) {
        backend = std::make_shared<SocketInterfaceWindowsIPv6>();
        needPortCfg = true;
        LOG(INFO) << "Chose IPv6 with address " << addressString;
    } else {
        backend = std::make_shared<SocketInterfaceWindowsLocal>();
        if (localSocketPath) {
            addressString = localSocketPath->string();
            LOG(INFO) << "Chose Unix Local socket with path " << addressString;
        } else {
            LOG(INFO) << "Chose Unix Local socket";
        }
    }
    if (needPortCfg) {
        std::string portStr;
        if (ConfigManager::getEnv(kPortEnvVar.data(), portStr)) {
            if (try_parse(portStr, &port)) {
                foundPort = true;
            }
        }
        if (!foundPort) {
            port = SocketInterfaceBase::kTgBotHostPort;
        }
        LOG(INFO) << "Using port " << port;
        backend->options.port = port;
    }
    if (!addressString.empty()) {
        backend->options.address = addressString;
    }
}
