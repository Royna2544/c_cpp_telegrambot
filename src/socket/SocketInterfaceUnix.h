#pragma once

#include <functional>

#include <internal/_logging_posix.h>
#include <internal/_FileDescriptor_posix.h>
#include "SocketInterfaceBase.h"
#include "TgBotSocket.h"

struct SocketInterfaceUnix : SocketInterfaceBase {
    using socket_handle_t = int;

    bool isValidSocketHandle(socket_handle_t handle) {
        return isValidFd(handle);
    };
    virtual socket_handle_t createClientSocket() = 0;
    virtual socket_handle_t createServerSocket() = 0;
    void writeToSocket(struct TgBotConnection conn) override;
    void forceStopListening(void) override;
    void startListening(const listener_callback_t& cb,
                        std::promise<bool>& createdPromise) override;

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
    ~SocketInterfaceUnixLocal() override = default;

   private:
    socket_handle_t makeSocket(bool client);
};

struct SocketHelperUnix {
    static void setSocketBindingToIface(
        const SocketInterfaceUnix::socket_handle_t sock, const char* iface);
};

// Implements POSIX socket interface - AF_INET
struct SocketInterfaceUnixIPv4 : SocketInterfaceUnix {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    bool isAvailable() override;
    virtual ~SocketInterfaceUnixIPv4() = default;

   private:
    void foreach_ipv4_interfaces(
        const std::function<void(const char*, const char*)> callback);
};

// Implements POSIX socket interface - AF_INET6
struct SocketInterfaceUnixIPv6 : SocketInterfaceUnix {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    bool isAvailable() override;
    virtual ~SocketInterfaceUnixIPv6() = default;

   private:
    void foreach_ipv6_interfaces(
        const std::function<void(const char*, const char*)> callback);
};