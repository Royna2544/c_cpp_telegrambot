#include <net/if.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "../SocketInterfaceUnix.h"

void SocketInterfaceUnixIPv4Darwin::setSocketBindingToIface(const socket_handle_t sfd, const char* iface) {
    const int index = if_nametoindex(iface);
    int opt = 1;
    
    if (index != 0) {
        setsockopt(sfd, IPPROTO_IP, IP_BOUND_IF, &index, sizeof(index));
    }
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
}
