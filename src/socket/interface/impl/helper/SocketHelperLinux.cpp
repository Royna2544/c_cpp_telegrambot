#include <net/if.h>

#include <impl/SocketPosix.hpp>

void SocketInterfaceUnix::bindToInterface(const socket_handle_t sock,
                                          const std::string& iface) {
#ifdef SO_BINDTODEVICE
    struct ifreq intf {};
    strncpy(intf.ifr_name, iface.c_str(), IFNAMSIZ);
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &intf, sizeof(intf)) <
        0) {
        PLOG(ERROR) << "setsockopt(SO_BINDTODEVICE) failed";
        return;
    }
#endif
    int opt = 1;

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt)) < 0) {
        PLOG(ERROR) << "setsockopt(SO_REUSEADDR/SO_REUSEPORT) failed";
        return;
    }
}