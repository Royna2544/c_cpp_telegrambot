#include <absl/log/log.h>

#include <SocketBase.hpp>
#include <TryParseStr.hpp>
#include <filesystem>
#include <libos/libfs.hpp>
#include <optional>
#include <string>

#include "SocketDescriptor_defs.hpp"

int SocketInterfaceBase::INetHelper::getPortNum() {
    try {
        return interface->options.port.get();
    } catch (const std::bad_optional_access& e) {
        LOG(ERROR) << "Could not get port number";
        return -1;
    }
}

bool SocketInterfaceBase::LocalHelper::canSocketBeClosed() {
    bool socketValid = true;

    if (!FS::exists(interface->options.address.get())) {
        LOG(WARNING) << "Socket file was deleted";
        socketValid = false;
    }
    return socketValid;
}

void SocketInterfaceBase::LocalHelper::cleanupServerSocket() {
    const auto path = std::filesystem::path(interface->options.address.get());
    DLOG(INFO) << "Cleaning up server socket...";
    FS::deleteFile(path);
}

void SocketInterfaceBase::LocalHelper::printRemoteAddress(socket_handle_t  /*socket*/) {
    LOG(INFO) << "Client connected via local socket";
}

std::filesystem::path SocketInterfaceBase::LocalHelper::getSocketPath() {
    static auto spath = std::filesystem::temp_directory_path() / "tgbot.sock";
    return spath;
}