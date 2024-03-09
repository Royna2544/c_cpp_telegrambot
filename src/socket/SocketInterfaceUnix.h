#pragma once

#include <functional>
#include <string>
#include <optional>

#include <Types.h>
#include "TgBotSocket.h"
#include "SocketInterfaceBase.h"

struct SocketInterfaceUnix : SocketInterfaceBase {
    using socket_handle_t = int;

    bool isValidSocketHandle(socket_handle_t handle)  {
        return isValidFd(handle);
    };
    virtual socket_handle_t createClientSocket() = 0;
    virtual socket_handle_t createServerSocket() = 0;
    virtual void cleanupServerSocket() {}
    void writeToSocket(struct TgBotConnection conn) override;
    void forceStopListening(void) override;
    void startListening(const listener_callback_t& cb, std::promise<bool>& createdPromise) override;
  
    virtual ~SocketInterfaceUnix() = default;
  protected:
    using kListenData = char;
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

// Implements POSIX socket interface - AF_INET
struct SocketInterfaceUnixIPv4 : SocketInterfaceUnix {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    virtual void setSocketBindingToIface(const socket_handle_t sock, const char* iface) = 0;

    virtual ~SocketInterfaceUnixIPv4() = default;
    
    void setDestinationAddress(const std::string addr) override {
        if (!addr.empty())
            inetaddr = addr;
        else
            inetaddr = kMyAddress.value();
    }
    constexpr static int kTgBotHostPort = 50000;
  private:
    void foreach_ipv4_interfaces(const std::function<void(const char*, const char*)> callback);
    std::optional<std::string> kMyAddress;
    std::string inetaddr;
};

struct SocketInterfaceUnixIPv4Linux : SocketInterfaceUnixIPv4 {
    void setSocketBindingToIface(const socket_handle_t sock, const char* iface) override;
    ~SocketInterfaceUnixIPv4Linux() override = default;
};
struct SocketInterfaceUnixIPv4Darwin : SocketInterfaceUnixIPv4 {
    void setSocketBindingToIface(const socket_handle_t sock, const char* iface) override;
    ~SocketInterfaceUnixIPv4Darwin() override = default;
};