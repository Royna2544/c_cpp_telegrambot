#include <BackendChooser.hpp>

#include "impl/SocketWindows.hpp"

SocketInterfaceBase* getClientBackend() {
    static BackendChooser<SocketInterfaceBase, SocketInterfaceWindowsIPv4,
                   SocketInterfaceWindowsIPv6, SocketInterfaceWindowsLocal>
        chooser;
    return chooser.getObject();
}