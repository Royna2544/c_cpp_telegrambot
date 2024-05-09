#include <BackendChooser.hpp>

#include "impl/SocketPosix.hpp"

SocketInterfaceBase* getClientBackend() {
    static BackendChooser<SocketInterfaceBase, SocketInterfaceUnixIPv4,
                   SocketInterfaceUnixIPv6, SocketInterfaceUnixLocal>
        chooser;
    return chooser.getObject();
}