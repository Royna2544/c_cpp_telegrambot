#include <absl/log/log.h>

#include <SocketBase.hpp>
#include <TryParseStr.hpp>
#include <filesystem>
#include <libos/libfs.hpp>
#include <string>

#include "SocketDescriptor_defs.hpp"

using Options = SocketInterfaceBase::Options;

int SocketInterfaceBase::INetHelper::getPortNum() {
    int ret = 0;
    std::string portStr = interface->getOptions(Options::DESTINATION_PORT);

    if (try_parse(portStr, &ret)) {
        return ret;
    }
    LOG(ERROR) << "Could not parse port number for string: " << std::quoted(portStr);
    return -1;
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

std::filesystem::path SocketInterfaceBase::LocalHelper::getSocketPath() {
    static auto spath = std::filesystem::temp_directory_path() / "tgbot.sock";
    return spath;
}