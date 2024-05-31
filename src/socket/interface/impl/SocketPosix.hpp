#pragma once

#include <absl/log/log.h>
#include <arpa/inet.h>
#include <internal/_FileDescriptor_posix.h>

#include <SocketBase.hpp>
#include <functional>

#include "SharedMalloc.hpp"

struct SocketInterfaceUnix : SocketInterfaceBase {
    bool isValidSocketHandle(socket_handle_t handle) override {
        return isValidFd(handle);
    };

    void writeToSocket(SocketConnContext context, SharedMalloc data) override;
    void forceStopListening(void) override;
    void startListening(socket_handle_t handle,
                        const listener_callback_t onNewData) override;
    bool closeSocketHandle(socket_handle_t& handle) override;
    bool setSocketOptTimeout(socket_handle_t handle, int timeout) override;

    std::optional<SharedMalloc> readFromSocket(
        SocketConnContext context,
        TgBotCommandPacketHeader::length_type length) override;

    SocketInterfaceUnix() = default;
    virtual ~SocketInterfaceUnix() = default;

   protected:
    pipe_t kListenTerminate{};
    int& listen_fd = kListenTerminate[0];
    int& notify_fd = kListenTerminate[1];
};

// Implements POSIX socket interface - AF_LOCAL
struct SocketInterfaceUnixLocal : SocketInterfaceUnix {
    std::optional<SocketConnContext> createClientSocket() override;
    std::optional<socket_handle_t> createServerSocket() override;
    void cleanupServerSocket() override;
    bool canSocketBeClosed() override;
    bool isSupported() override;
    ~SocketInterfaceUnixLocal() override = default;
    void doGetRemoteAddr(socket_handle_t s) override;

   private:
    bool createLocalSocket(SocketConnContext* ctx);
};

template <typename NetworkAddr>
concept IsNetworkAddrType = std::is_same_v<NetworkAddr, struct sockaddr_in> ||
                            std::is_same_v<NetworkAddr, struct sockaddr_in6>;

namespace SocketHelperUnix {

void setSocketBindingToIface(const socket_handle_t sock, const char* iface);

template <typename AddrInfo, typename IPAddr, int offset>

    requires IsNetworkAddrType<AddrInfo>
IPAddr* getIPAddr(AddrInfo* addr) {
    return reinterpret_cast<IPAddr*>(reinterpret_cast<char*>(addr) + offset);
}

template <typename IPAddr, int family, int addr_str_len>
void printToBuf(IPAddr* addr, std::array<char, addr_str_len>& buf) {
    inet_ntop(family, addr, buf.data(), addr_str_len);
}

template <typename AddrInfo, typename IPAddr, int family, int addr_str_len,
          int offset>
    requires IsNetworkAddrType<AddrInfo>
struct GetRemoteAddress {
    static void run(socket_handle_t s) {
        AddrInfo addr;
        std::array<char, addr_str_len> buf;
        socklen_t len = sizeof(AddrInfo);
        IPAddr* addr_ptr = getIPAddr<AddrInfo, IPAddr, offset>(&addr);
        if (getpeername(s, (struct sockaddr*)&addr, &len) != 0) {
            PLOG(ERROR) << "Get connected peer address failed";
        } else {
            printToBuf<IPAddr, family, addr_str_len>(addr_ptr, buf);
            LOG(INFO) << "Client connected, its addr: " << buf.data();
        }
    }
};

template <typename AddrInfo, typename IPAddr, int family, int addr_str_len,
          int offset>
    requires IsNetworkAddrType<AddrInfo>
struct ForEachInterfaces {
    static void run(
        const std::function<void(const char* iface, const char* addr)>
            callback) {
        struct ifaddrs *addrs = nullptr, *tmp = nullptr;
        getifaddrs(&addrs);
        tmp = addrs;
        while (tmp) {
            if (tmp->ifa_addr && tmp->ifa_addr->sa_family == family) {
                std::array<char, addr_str_len> ipStr;
                AddrInfo* info = reinterpret_cast<AddrInfo*>(tmp->ifa_addr);
                IPAddr* pAddr = getIPAddr<AddrInfo, IPAddr, offset>(info);

                printToBuf<IPAddr, family, addr_str_len>(pAddr, ipStr);
                callback(tmp->ifa_name, ipStr.data());
            }
            tmp = tmp->ifa_next;
        }
        freeifaddrs(addrs);
    }
};
using foreach_ipv4_interfaces =
    ForEachInterfaces<struct sockaddr_in, in_addr, AF_INET, INET_ADDRSTRLEN,
                      offsetof(struct sockaddr_in, sin_addr)>;
using foreach_ipv6_interfaces =
    ForEachInterfaces<struct sockaddr_in6, in_addr, AF_INET6, INET6_ADDRSTRLEN,
                      offsetof(struct sockaddr_in6, sin6_addr)>;
using getremoteaddr_ipv4 =
    GetRemoteAddress<struct sockaddr_in, in_addr, AF_INET, INET_ADDRSTRLEN,
                     offsetof(struct sockaddr_in, sin_addr)>;
using getremoteaddr_ipv6 =
    GetRemoteAddress<struct sockaddr_in6, in_addr, AF_INET6, INET6_ADDRSTRLEN,
                     offsetof(struct sockaddr_in6, sin6_addr)>;
}  // namespace SocketHelperUnix

// Implements POSIX socket interface - AF_INET
struct SocketInterfaceUnixIPv4 : SocketInterfaceUnix {
    std::optional<SocketConnContext> createClientSocket() override;
    std::optional<socket_handle_t> createServerSocket() override;
    bool isSupported() override;
    virtual ~SocketInterfaceUnixIPv4() = default;
    void doGetRemoteAddr(socket_handle_t s) override;
};

// Implements POSIX socket interface - AF_INET6
struct SocketInterfaceUnixIPv6 : SocketInterfaceUnix {
    std::optional<SocketConnContext> createClientSocket() override;
    std::optional<socket_handle_t> createServerSocket() override;
    bool isSupported() override;
    virtual ~SocketInterfaceUnixIPv6() = default;
    void doGetRemoteAddr(socket_handle_t s) override;
};