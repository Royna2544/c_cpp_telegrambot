#include <net/if.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <impl/SocketPosix.hpp>

void SocketInterfaceUnix::bindToInterface(const socket_handle_t sock,
                                          const std::string& iface) {
    const int index = if_nametoindex(iface.c_str());
    int opt = 1;

    if (index != 0) {
        setsockopt(sock, IPPROTO_IP, IP_BOUND_IF, &index, sizeof(index));
    }
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
               sizeof(opt));
}
