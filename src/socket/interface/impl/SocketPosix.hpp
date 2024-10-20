#pragma once

#include <trivial_helpers/_FileDescriptor_posix.h>

#include <SharedMalloc.hpp>
#include <SocketBase.hpp>

#include "SocketDescriptor_defs.hpp"

struct SocketInterfaceUnix : SocketInterfaceBase {
    bool isValidSocketHandle(socket_handle_t handle) override {
        return isValidFd(handle);
    };

    bool writeToSocket(SocketConnContext context, SharedMalloc data) override;
    void forceStopListening(void) override;
    void startListening(socket_handle_t handle,
                        const listener_callback_t onNewData) override;
    bool closeSocketHandle(socket_handle_t& handle) override;
    bool setSocketOptTimeout(socket_handle_t handle, int timeout) override;

    std::optional<SharedMalloc> readFromSocket(SocketConnContext context,
                                               buffer_len_t length) override;

    SocketInterfaceUnix() : posixHelper(this) {}
    ~SocketInterfaceUnix() override = default;

    struct PosixHelper {
        // SOCK_DGRAM or SOCK_STREAM?
        int getSocketType();
        // Connection timeout for clients are enabled?
        bool connectionTimeoutEnabled();

        // Handle connection timeout. Specific works before connect() in client
        static void handleConnectTimeoutPre(socket_handle_t socket);
        // Handle connection timeout. Specific works after connect() in client
        bool handleConnectTimeoutPost(socket_handle_t socket);

        explicit PosixHelper(SocketInterfaceUnix* _interface)
            : interface(_interface) {}

       private:
        SocketInterfaceUnix* interface;
    } posixHelper;

   protected:
    Pipe kListenTerminate{};
    static void bindToInterface(const socket_handle_t sock,
                                const std::string& iface);
};

// Implements POSIX socket interface - AF_LOCAL
struct SocketInterfaceUnixLocal : SocketInterfaceUnix {
    std::optional<SocketConnContext> createClientSocket() override;
    std::optional<socket_handle_t> createServerSocket() override;
    void cleanupServerSocket() override;
    bool canSocketBeClosed() override;
    void printRemoteAddress(socket_handle_t s) override;
    SocketInterfaceUnixLocal() = default;
    ~SocketInterfaceUnixLocal() override = default;

   private:
    bool createLocalSocket(SocketConnContext* ctx);
};

// Implements POSIX socket interface - AF_INET
struct SocketInterfaceUnixIPv4 : SocketInterfaceUnix {
    std::optional<SocketConnContext> createClientSocket() override;
    std::optional<socket_handle_t> createServerSocket() override;
    ~SocketInterfaceUnixIPv4() override = default;
    void printRemoteAddress(socket_handle_t s) override;
};

// Implements POSIX socket interface - AF_INET6
struct SocketInterfaceUnixIPv6 : SocketInterfaceUnix {
    std::optional<SocketConnContext> createClientSocket() override;
    std::optional<socket_handle_t> createServerSocket() override;
    ~SocketInterfaceUnixIPv6() override = default;
    void printRemoteAddress(socket_handle_t s) override;
};
