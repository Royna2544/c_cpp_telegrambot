#include <absl/log/log.h>

#include <SocketBase.hpp>
#include <TryParseStr.hpp>
#include <filesystem>
#include <libfs.hpp>
#include <optional>
#include <string>
#include <system_error>

#include "SocketDescriptor_defs.hpp"

int SocketInterfaceBase::INetHelper::getPortNum() {
    try {
        return _interface->options.port.get();
    } catch (const std::bad_optional_access& e) {
        LOG(ERROR) << "Could not get port number";
        return -1;
    }
}

bool SocketInterfaceBase::LocalHelper::canSocketBeClosed() {
    bool socketValid = true;

    if (!std::filesystem::exists(_interface->options.address.get())) {
        LOG(WARNING) << "Socket file was deleted";
        socketValid = false;
    }
    return socketValid;
}

void SocketInterfaceBase::LocalHelper::cleanupServerSocket() {
    const auto path = std::filesystem::path(_interface->options.address.get());
    DLOG(INFO) << "Cleaning up server socket...";
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
        LOG(ERROR) << "Failed to remove socket file: " << ec.message();
    }
}

void SocketInterfaceBase::LocalHelper::printRemoteAddress(socket_handle_t  /*socket*/) {
    LOG(INFO) << "Client connected via local socket";
}

std::filesystem::path SocketInterfaceBase::LocalHelper::getSocketPath() {
    static auto spath = std::filesystem::temp_directory_path() / "tgbot.sock";
    return spath;
}