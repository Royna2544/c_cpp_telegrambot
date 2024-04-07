#include <absl/log/log.h>

#include <libos/libfs.hpp>
#include <string>

#include "../SocketInterfaceBase.h"

bool SocketHelperCommon::_isAvailable(SocketInterfaceBase *it,
                                      const char *envVar) {
    char *addr = getenv(envVar);
    int portNum = SocketInterfaceBase::kTgBotHostPort;

    if (!addr) {
        LOG(INFO) << envVar << " is not set, isAvailable false";
        return false;
    }
    it->setOptions(SocketInterfaceBase::Options::DESTINATION_ADDRESS, addr,
                   true);
    if (const char *port = getenv(kPortEnvVar); port != nullptr) {
        try {
            portNum = std::stoi(port);
            DLOG(INFO) << kPortEnvVar << " is set";
        } catch (...) {
            LOG(ERROR) << "Illegal value for " << kPortEnvVar << ": " << port;
        }
        LOG(INFO) << "Chosen port: " << portNum;
    }
    it->setOptions(SocketInterfaceBase::Options::DESTINATION_PORT,
                   std::to_string(portNum), true);
    return true;
}

bool SocketHelperCommon::isAvailableIPv4(SocketInterfaceBase *it) {
    return _isAvailable(it, kIPv4EnvVar);
}

bool SocketHelperCommon::isAvailableIPv6(SocketInterfaceBase *it) {
    return _isAvailable(it, kIPv6EnvVar);
}

int SocketHelperCommon::getPortNumInet(SocketInterfaceBase *it) {
    return stoi(it->getOptions(SocketInterfaceBase::Options::DESTINATION_PORT));
}

bool SocketHelperCommon::isAvailableLocalSocket() {
    DLOG(INFO) << "Choosing local socket";
    return true;
}

bool SocketHelperCommon::canSocketBeClosedLocalSocket(SocketInterfaceBase *it) {
    bool socketValid = true;

    if (!FS::exists(it->getOptions(
            SocketInterfaceBase::Options::DESTINATION_ADDRESS))) {
        LOG(WARNING) << "Socket file was deleted";
        socketValid = false;
    }
    return socketValid;
}

void SocketHelperCommon::cleanupServerSocketLocalSocket(
    SocketInterfaceBase *it) {
    std::filesystem::remove(
        it->getOptions(SocketInterfaceBase::Options::DESTINATION_ADDRESS));
}
