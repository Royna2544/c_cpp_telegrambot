#include <net/if.h>

#include <impl/SocketPosix.hpp>

void SocketInterfaceUnix::bindToInterface(const socket_handle_t sock,
                                          const std::string& iface) {
    struct ifreq intf {};
    int opt = 1;

    memset(&intf, 0, sizeof(intf));
    strncpy(intf.ifr_ifrn.ifrn_name, iface.c_str(), IFNAMSIZ);
    setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &intf, sizeof(intf));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
               sizeof(opt));
}
