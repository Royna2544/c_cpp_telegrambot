#include <string>

#include "ClientBackend.hpp"
#include "ConfigManager.h"
#include "impl/SocketWindows.hpp"

SocketClientWrapper::SocketClientWrapper() {
    std::string value;
    std::string port;
    if (ConfigManager::getEnv(kIPv4EnvVar.data(), value)) {
        backend = std::make_shared<SocketInterfaceWindowsIPv4>();
        LOG(INFO) << "Chose IPv4 with address " << value;
    } else if (ConfigManager::getEnv(kIPv6EnvVar.data(), value)) {
        backend = std::make_shared<SocketInterfaceWindowsIPv6>();
        LOG(INFO) << "Chose IPv6 with address " << value;
    } else {
        backend = std::make_shared<SocketInterfaceWindowsLocal>();
        LOG(INFO) << "Chose Windows Local socket";
    }
    if (!ConfigManager::getEnv(kPortEnvVar.data(), port)) {
        port = std::to_string(SocketInterfaceBase::kTgBotHostPort);
    }
    LOG(INFO) << "Using port " << port;
    backend->setOptions(SocketInterfaceBase::Options::DESTINATION_ADDRESS, value);
    backend->setOptions(SocketInterfaceBase::Options::DESTINATION_PORT, port);
}
