#include <ConfigManager.hpp>
#include <TryParseStr.hpp>
#include <impl/SocketWindows.hpp>
#include <string>

#include "ClientBackend.hpp"

SocketClientWrapper::SocketClientWrapper(
    std::optional<std::filesystem::path> localSocketPath) {
    std::string addressString;
    int port = 0;
    bool needPortCfg = false;
    bool foundPort = false;
    Env env;
    if (env[kIPv4EnvVar].assign(addressString)) {
        backend = std::make_shared<SocketInterfaceWindowsIPv4>();
        needPortCfg = true;
        LOG(INFO) << "Chose IPv4 with address " << addressString;
    } else if (env[kIPv6EnvVar].assign(addressString)) {
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
        if (env[kPortEnvVar].assign(portStr)) {
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
