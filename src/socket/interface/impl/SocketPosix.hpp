#pragma once

#include <absl/log/log.h>
#include <arpa/inet.h>
#include <internal/_FileDescriptor_posix.h>

#include <SocketBase.hpp>

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

    std::optional<SharedMalloc> readFromSocket(SocketConnContext context,
                                               buffer_len_t length) override;

    SocketInterfaceUnix() = default;
    ~SocketInterfaceUnix() override = default;

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
    void doGetRemoteAddr(socket_handle_t s) override;
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
    void doGetRemoteAddr(socket_handle_t s) override;
};

// Implements POSIX socket interface - AF_INET6
struct SocketInterfaceUnixIPv6 : SocketInterfaceUnix {
    std::optional<SocketConnContext> createClientSocket() override;
    std::optional<socket_handle_t> createServerSocket() override;
    ~SocketInterfaceUnixIPv6() override = default;
    void doGetRemoteAddr(socket_handle_t s) override;
};
