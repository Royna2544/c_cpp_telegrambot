#include <absl/log/log.h>
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#include <SocketBase.hpp>

#include "SharedMalloc.hpp"
#include "SocketDescriptor_defs.hpp"

struct SocketInterfaceWindows : SocketInterfaceBase {
    bool isValidSocketHandle(socket_handle_t handle) override {
        return handle != INVALID_SOCKET;
    };
    static std::string WSALastErrorStr();

    bool writeToSocket(SocketConnContext context, SharedMalloc data) override;
    void forceStopListening(void) override;
    void startListening(socket_handle_t handle,
                        const listener_callback_t onNewData) override;
    bool closeSocketHandle(socket_handle_t& handle) override;
    bool setSocketOptTimeout(socket_handle_t handle, int timeout) override;
    std::optional<SharedMalloc> readFromSocket(
        SocketConnContext context,
        TgBotSocket::PacketHeader::length_type length) override;

    struct WinHelper {
        explicit WinHelper(SocketInterfaceWindows* interface)
            : interface(interface) {}
        bool createInetSocketAddr(SocketConnContext& context);
        bool createInet6SocketAddr(SocketConnContext& context);
        template <typename T, int family, typename addr_t, int addrbuf_len,
                  int offset>
            requires std::is_same_v<T, struct sockaddr_in> ||
                     std::is_same_v<T, struct sockaddr_in6>
        static void doGetRemoteAddrInet(socket_handle_t s) {
            T addr;
            std::array<char, addrbuf_len> ipStr = {};
            socklen_t len = sizeof(T);
            auto* addr_ptr = reinterpret_cast<addr_t*>(
                reinterpret_cast<char*>(&addr) + offset);
            if (getpeername(s, (struct sockaddr*)&addr, &len) != 0) {
                LOG(ERROR) << "getpeername failed: " << WSALastErrorStr();
            } else {
                inet_ntop(family, addr_ptr, ipStr.data(), addrbuf_len);
                LOG(INFO) << "Client connected, its addr: " << ipStr.data();
            }
        }

       private:
        SocketInterfaceWindows* interface;
    } win_helper;

    constexpr static DWORD kWSAVersion = MAKEWORD(2, 2);

    SocketInterfaceWindows() : win_helper(this) {
        WSADATA wsaData;
        if (WSAStartup(kWSAVersion, &wsaData) != 0) {
            LOG(ERROR) << "WSAStartup failed: " << WSALastErrorStr();
            throw std::runtime_error("WSAStartup failed");
        }
    };
    ~SocketInterfaceWindows() override { WSACleanup(); };

   private:
    std::atomic_bool kRun = true;
    WSAData data{};
};

static inline auto WSAEStr = SocketInterfaceWindows::WSALastErrorStr;

struct SocketInterfaceWindowsLocal : SocketInterfaceWindows {
    std::optional<SocketConnContext> createClientSocket() override;
    std::optional<socket_handle_t> createServerSocket() override;
    void cleanupServerSocket() override;
    bool canSocketBeClosed() override;
    void doGetRemoteAddr(socket_handle_t s) override;

   private:
    bool createLocalSocket(SocketConnContext* ctx);
};

struct SocketInterfaceWindowsIPv4 : SocketInterfaceWindows {
    std::optional<SocketConnContext> createClientSocket() override;
    std::optional<socket_handle_t> createServerSocket() override;
    void doGetRemoteAddr(socket_handle_t s) override;

   private:
    socket_handle_t makeSocket(bool is_client);
};

struct SocketInterfaceWindowsIPv6 : SocketInterfaceWindows {
    std::optional<SocketConnContext> createClientSocket() override;
    std::optional<socket_handle_t> createServerSocket() override;
    void doGetRemoteAddr(socket_handle_t s) override;

   private:
    socket_handle_t makeSocket(bool is_client);
};
