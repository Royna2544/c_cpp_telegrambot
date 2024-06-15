#include <absl/log/log.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <array>
#include <cstddef>
#include <functional>
#include <string_view>

#include "SocketDescriptor_defs.hpp"

/**
 * @brief Retrieves the length of the string representation of an address of the
 * specified address family.
 *
 * This function returns the length of the string representation of an address
 * of the specified address family. It uses the `getAddrStrLen` macro to
 * determine the length based on the address family (AF_INET or AF_INET6).
 *
 * @param af The address family for which the length of the string
 * representation is to be obtained.
 *
 * @return The length of the string representation of an address of the
 * specified address family.
 */
template <int af>
consteval size_t getAddrStrLen() {
    if constexpr (af == AF_INET) {
        return INET_ADDRSTRLEN;
    } else if constexpr (af == AF_INET6) {
        return INET6_ADDRSTRLEN;
    } else {
        return 0;
    }
}

template <typename SockAddr, size_t N>
const char* getAddrStr(const SockAddr& addr, std::array<char, N>& addrStr) = delete;

template <size_t N>
const char* getAddrStr(const sockaddr_in& addr, std::array<char, N>& addrStr) {
    return inet_ntop(AF_INET, &addr.sin_addr, addrStr.data(), N);
}

template <size_t N>
const char* getAddrStr(const sockaddr_in6& addr, std::array<char, N>& addrStr) {
    return inet_ntop(AF_INET6, &addr.sin6_addr, addrStr.data(), N);
}

/**
 * @brief Prints the remote address of the given socket.
 *
 * This function retrieves the remote address of the given socket and prints it.
 * It uses the `getpeername` system call to obtain the remote address and the
 * `inet_ntop` function to convert the address to a string. If either of these
 * operations fails, an error message is logged.
 *
 * @param sock The socket handle to retrieve the remote address from.
 */
template <typename SockAddrT, /* e.g. sockaddr_in */
          typename AddrT,     /* e.g. addr_in */
          int af /* e.g. AF_INET */>
    requires(af == AF_INET || af == AF_INET6)
void printRemoteAddress(socket_handle_t sock) {
    constexpr size_t addrStrLen = getAddrStrLen<af>();
    std::array<char, addrStrLen> addrStr{};
    SockAddrT addr;
    socklen_t addrLen = sizeof(addr);

    if (getpeername(sock, reinterpret_cast<sockaddr*>(&addr), &addrLen) != 0) {
        PLOG(ERROR) << "getpeername failed";
    }
    if (getAddrStr(addr, addrStr) == nullptr) {
        LOG(ERROR) << "inet_ntop failed";
    }
    LOG(INFO) << "Remote address: " << addrStr.data();
}

struct INetInterfaceData {
    std::string name;
    std::string addr;
};

/**
 * @brief Iterates through all network interfaces and calls the provided
 * callback function for each one.
 *
 * This function retrieves a list of all network interfaces and iterates through
 * them. For each interface, it checks if the interface's address family matches
 * the specified address family (AF_INET or AF_INET6). If it does, it constructs
 * an INetInterfaceData object containing the interface's name and address, and
 * calls the provided callback function with this object.
 *
 * @param callback The function to be called for each network interface.
 */
template <typename SockAddrT, /* e.g. sockaddr_in */
          typename AddrT,     /* e.g. addr_in */
          int af /* e.g. AF_INET */>
    requires(af == AF_INET || af == AF_INET6)
void forEachINetAddress(
    const std::function<void(const INetInterfaceData&)>& callback) {
    struct ifaddrs *addrs = nullptr, *tmp = nullptr;
    getifaddrs(&addrs);
    tmp = addrs;
    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == af) {
            constexpr auto addrStrLen = getAddrStrLen<af>();
            std::array<char, addrStrLen> addrStr{};
            const auto addr = *reinterpret_cast<SockAddrT*>(tmp->ifa_addr);

            if (getAddrStr(addr, addrStr) == nullptr) {
                LOG(ERROR) << "inet_ntop failed";
            }
            INetInterfaceData data{};
            data.name = tmp->ifa_name;
            data.addr = addrStr.data();
            callback(data);
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(addrs);
}

constexpr std::string_view kLocalInterface = "lo";