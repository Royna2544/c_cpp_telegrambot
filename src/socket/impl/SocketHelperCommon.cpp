#include "../SocketInterfaceBase.h"
#include <libos/libfs.hpp>

bool SocketHelperCommon::_isAvailable(SocketInterfaceBase *it, const char *envVar) {
    char* addr = getenv(envVar);
    if (!addr) {
        LOG_D("%s is not set, isAvailable false", envVar);
        return false;
    }
    it->setOptions(SocketInterfaceBase::Options::DESTINATION_ADDRESS, addr, true);
    return true;
}

bool SocketHelperCommon::isAvailableIPv4(SocketInterfaceBase *it) {
    return _isAvailable(it, kIPv4EnvVar);
}

bool SocketHelperCommon::isAvailableIPv6(SocketInterfaceBase *it) {
    return _isAvailable(it, kIPv6EnvVar);
}
bool SocketHelperCommon::canSocketBeClosedLocalSocket(SocketInterfaceBase *it) {
    bool socketValid = true;

    if (!fileExists(it->getOptions(SocketInterfaceBase::Options::DESTINATION_ADDRESS))) {
        LOG_W("Socket file was deleted");
        socketValid = false;
    }
    return socketValid;
}

void SocketHelperCommon::cleanupServerSocketLocalSocket(SocketInterfaceBase *it) {
    std::filesystem::remove(it->getOptions(SocketInterfaceBase::Options::DESTINATION_ADDRESS));
}