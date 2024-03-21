#pragma once

#include <arpa/inet.h>
#include <internal/_FileDescriptor_posix.h>
#include <internal/_logging_posix.h>

#include <functional>

#include "SocketInterfaceBase.h"
#include "TgBotSocket.h"

using socket_handle_t = int;

struct SocketInterfaceUnix : SocketInterfaceBase {
    bool isValidSocketHandle(socket_handle_t handle) {
        return isValidFd(handle);
    };
    virtual socket_handle_t createClientSocket() = 0;
    virtual socket_handle_t createServerSocket() = 0;
    void writeToSocket(struct TgBotConnection conn) override;
    void forceStopListening(void) override;
    void startListening(const listener_callback_t& cb,
                        const result_callback_t& createdPromise) override;
    virtual void doGetRemoteAddr(socket_handle_t s) = 0;

    virtual ~SocketInterfaceUnix() = default;

   protected:
    pipe_t kListenTerminate;
    int& listen_fd = kListenTerminate[0];
    int& notify_fd = kListenTerminate[1];
};

// Implements POSIX socket interface - AF_LOCAL
struct SocketInterfaceUnixLocal : SocketInterfaceUnix {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    void cleanupServerSocket() override;
    bool canSocketBeClosed() override;
    bool isAvailable() override;
    ~SocketInterfaceUnixLocal() override = default;
    void doGetRemoteAddr(socket_handle_t s) override;

   private:
    socket_handle_t makeSocket(bool client);
};

struct SocketHelperUnix {
    static void setSocketBindingToIface(const socket_handle_t sock,
                                        const char* iface);

    template <typename T, int family, typename addr_t, int addrbuf_len, int offset>
        requires std::is_same_v<T, struct sockaddr_in> ||
                 std::is_same_v<T, struct sockaddr_in6>
    static void doGetRemoteAddrInet(socket_handle_t s) {
        T addr;
        char ipStr[addrbuf_len] = {};
        socklen_t len = sizeof(T);
        addr_t* addr_ptr =
            reinterpret_cast<addr_t*>(reinterpret_cast<char*>(&addr) + offset);
        if (getpeername(s, (struct sockaddr*)&addr, &len) != 0) {
            PLOG_E("Get connected peer address failed");
        } else {
            inet_ntop(family, addr_ptr, ipStr, addrbuf_len);
            LOG(LogLevel::DEBUG, "Client connected, its addr: %s", ipStr);
        }
    }
};

// Implements POSIX socket interface - AF_INET
struct SocketInterfaceUnixIPv4 : SocketInterfaceUnix {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    bool isAvailable() override;
    void setupExitVerification() override{};
    void stopListening(const std::string& e) override;
    virtual ~SocketInterfaceUnixIPv4() = default;
    void doGetRemoteAddr(socket_handle_t s) override;

   private:
    void foreach_ipv4_interfaces(
        const std::function<void(const char*, const char*)> callback);
};

// Implements POSIX socket interface - AF_INET6
struct SocketInterfaceUnixIPv6 : SocketInterfaceUnix {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    bool isAvailable() override;
    void setupExitVerification() override{};
    void stopListening(const std::string& e) override;
    virtual ~SocketInterfaceUnixIPv6() = default;
    void doGetRemoteAddr(socket_handle_t s) override;

   private:
    void foreach_ipv6_interfaces(
        const std::function<void(const char*, const char*)> callback);
};