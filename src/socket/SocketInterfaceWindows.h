#include <winsock2.h>
#include <ws2tcpip.h>

#include "SocketInterfaceBase.h"

#define WSALOG_E(fmt) \
    LOG(LogLevel::ERROR, fmt ": %s", SocketInterfaceWindows::strWSAError(WSAGetLastError()))

using socket_handle_t = SOCKET;

struct SocketInterfaceWindows : SocketInterfaceBase {
    static bool isValidSocketHandle(socket_handle_t handle) {
        return handle != INVALID_SOCKET;
    };
    static char* strWSAError(const int errcode);

    virtual socket_handle_t createClientSocket() = 0;
    virtual socket_handle_t createServerSocket() = 0;
    void writeToSocket(struct TgBotConnection conn) override;
    void forceStopListening(void) override;
    void startListening(const listener_callback_t& cb,
                        std::promise<bool>& createdPromise) override;

    virtual ~SocketInterfaceWindows() = default;

   private:
    std::atomic_bool kRun = true;
};

struct SocketInterfaceWindowsLocal : SocketInterfaceWindows {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    void cleanupServerSocket() override;
    bool canSocketBeClosed() override;
    bool isAvailable() override;

   private:
    socket_handle_t makeSocket(bool is_client);
};

struct SocketInterfaceWindowsIPv4 : SocketInterfaceWindows {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    bool isAvailable() override;

   private:
    socket_handle_t makeSocket(bool is_client);
};

struct SocketInterfaceWindowsIPv6 : SocketInterfaceWindows {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    bool isAvailable() override;

   private:
    socket_handle_t makeSocket(bool is_client);
};

struct SocketHelperWindows {
    static bool createInetSocketAddr(socket_handle_t *socket, struct sockaddr_in *addr);
    static bool createInet6SocketAddr(socket_handle_t *socket, struct sockaddr_in6 *addr);
};