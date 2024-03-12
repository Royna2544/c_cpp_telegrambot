#include "../SocketInterfaceBase.h"
#include <libos/libfs.h>

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
bool SocketHelperCommon::canSocketBeClosedLocalSocket() {
    bool socketValid = true;

    if (!fileExists(SOCKET_PATH)) {
        LOG_W("Socket file was deleted");
        socketValid = false;
    }
    return socketValid;
}

void SocketHelperCommon::cleanupServerSocketLocalSocket() {
    std::filesystem::remove(SOCKET_PATH);
}