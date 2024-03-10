#include <net/if.h>

#include "../SocketInterfaceUnix.h"

void SocketHelperUnix::setSocketBindingToIface(const SocketInterfaceUnix::socket_handle_t sfd, const char* iface) {
    struct ifreq intf {};
    int opt = 1;

    memset(&intf, 0, sizeof(intf));
    strncpy(intf.ifr_ifrn.ifrn_name, iface, IFNAMSIZ);
    setsockopt(sfd, SOL_SOCKET, SO_BINDTODEVICE, &intf, sizeof(intf));
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
}
