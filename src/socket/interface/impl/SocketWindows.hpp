#include <absl/log/log.h>
#include <inaddr.h>
#include <socket/TgBotSocket.h>
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#include <SocketBase.hpp>

extern void WSALOG_E(const char* msg);

struct SocketInterfaceWindows : SocketInterfaceBase {
    bool isValidSocketHandle(socket_handle_t handle) override {
        return handle != INVALID_SOCKET;
    };
    static char* strWSAError(const int errcode);

    void writeToSocket(struct SocketData sdata) override;
    void forceStopListening(void) override;
    void startListening(const listener_callback_t onNewData) override;
    char* getLastErrorMessage() override;
    std::optional<SocketData> readFromSocket(
        socket_handle_t handle, SocketData::length_type length) override;
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
    void doGetRemoteAddr(socket_handle_t s) override;

   private:
    socket_handle_t makeSocket(bool is_client);
};

struct SocketInterfaceWindowsIPv4 : SocketInterfaceWindows {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    bool isAvailable() override;
    void doGetRemoteAddr(socket_handle_t s) override;

   private:
    socket_handle_t makeSocket(bool is_client);
};

struct SocketInterfaceWindowsIPv6 : SocketInterfaceWindows {
    socket_handle_t createClientSocket() override;
    socket_handle_t createServerSocket() override;
    bool isAvailable() override;
    void doGetRemoteAddr(socket_handle_t s) override;

   private:
    socket_handle_t makeSocket(bool is_client);
};

struct SocketHelperWindows {
    static bool createInetSocketAddr(socket_handle_t* socket,
                                     struct sockaddr_in* addr,
                                     SocketInterfaceWindows* it);
    static bool createInet6SocketAddr(socket_handle_t* socket,
                                      struct sockaddr_in6* addr,
                                      SocketInterfaceWindows* it);
    template <typename T, int family, typename addr_t, int addrbuf_len,
              int offset>
        requires std::is_same_v<T, struct sockaddr_in> ||
                 std::is_same_v<T, struct sockaddr_in6>
    static void doGetRemoteAddrInet(socket_handle_t s) {
        T addr;
        char ipStr[addrbuf_len] = {};
        socklen_t len = sizeof(T);
        addr_t* addr_ptr =
            reinterpret_cast<addr_t*>(reinterpret_cast<char*>(&addr) + offset);
        if (getpeername(s, (struct sockaddr*)&addr, &len) != 0) {
            WSALOG_E("Get connected peer address failed");
        } else {
            inet_ntop(family, addr_ptr, ipStr, addrbuf_len);
            LOG(INFO) << "Client connected, its addr: " << ipStr;
        }
    }
};