#include <absl/log/log.h>

#include <SocketBase.hpp>
#include <TryParseStr.hpp>
#include <filesystem>
#include <libos/libfs.hpp>
#include <string>
#include "SocketDescriptor_defs.hpp"

using Options = SocketInterfaceBase::Options;

bool SocketInterfaceBase::INetHelper::_isSupported(const std::string_view envVar) {
    char *addr = getenv(envVar.data());
    int portNum = SocketInterfaceBase::kTgBotHostPort;

    if (addr == nullptr) {
        LOG(INFO) << envVar << " is not set, isSupported false";
        return false;
    }
    interface->setOptions(Options::DESTINATION_ADDRESS, addr, true);
    if (const char *port = getenv(kPortEnvVar.data()); port != nullptr) {
        if (!try_parse(port, &portNum)) {
            LOG(ERROR) << "Illegal value for " << kPortEnvVar << ": " << port;
        }
        LOG(INFO) << "Chosen port: " << portNum;
    }
    interface->setOptions(Options::DESTINATION_PORT, std::to_string(portNum),
                          true);
    return true;
}

bool SocketInterfaceBase::INetHelper::isSupportedIPv4() {
    return _isSupported(kIPv4EnvVar);
}

bool SocketInterfaceBase::INetHelper::isSupportedIPv6() {
    return _isSupported(kIPv6EnvVar);
}

int SocketInterfaceBase::INetHelper::getPortNum() {
    int ret = 0;

    if (try_parse(interface->getOptions(Options::DESTINATION_PORT), &ret)) {
        return ret;
    }
    return SocketInterfaceBase::kTgBotHostPort;
}

bool SocketInterfaceBase::LocalHelper::isSupported() {
    DLOG(INFO) << "Choosing local socket";
    return true;
}

bool SocketInterfaceBase::LocalHelper::canSocketBeClosed() {
    bool socketValid = true;

    if (!FS::exists(interface->getOptions(Options::DESTINATION_ADDRESS))) {
        LOG(WARNING) << "Socket file was deleted";
        socketValid = false;
    }
    return socketValid;
}

void SocketInterfaceBase::LocalHelper::cleanupServerSocket() {
    const auto path = std::filesystem::path(
                          interface->getOptions(Options::DESTINATION_ADDRESS));
    DLOG(INFO) << "Cleaning up server socket...";
    FS::deleteFile(path);
}

void SocketInterfaceBase::LocalHelper::doGetRemoteAddr(socket_handle_t socket) {
    LOG(INFO) << "Client connected via local socket";
}