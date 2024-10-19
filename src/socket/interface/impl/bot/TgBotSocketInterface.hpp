#include <TgBotPPImplExports.h>

#include <ManagedThreads.hpp>
#include <SocketBase.hpp>
#include <api/TgBotApi.hpp>
#include <functional>
#include <memory>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
#elif defined(WINDOWS_BUILD)
#include <ws2tcpip.h>
#endif

#include "TgBotSocketFileHelperNew.hpp"
#include "TgBotSocket_Export.hpp"

using TgBotSocket::callback::GenericAck;
using TgBotSocket::callback::UploadFileDryCallback;

struct TgBotPPImpl_API SocketInterfaceTgBot : ManagedThreadRunnable {
    void handlePacket(SocketConnContext ctx, TgBotSocket::Packet pkt);

    void runFunction() override;

    explicit SocketInterfaceTgBot(
        std::shared_ptr<SocketInterfaceBase> _interface,
        InstanceClassBase<TgBotApi>::pointer_type _api,
        std::shared_ptr<SocketFile2DataHelper> helper);

   private:
    std::shared_ptr<SocketInterfaceBase> interface = nullptr;
    InstanceClassBase<TgBotApi>::pointer_type api = nullptr;
    std::shared_ptr<SocketFile2DataHelper> helper;

    std::chrono::system_clock::time_point startTp =
        std::chrono::system_clock::now();
    using command_handler_fn_t = std::function<void(
        socket_handle_t source, void* addr, socklen_t len, const void* data)>;

    // Command handlers
    GenericAck handle_WriteMsgToChatId(const void* ptr);
    GenericAck handle_SendFileToChatId(const void* ptr);
    static GenericAck handle_CtrlSpamBlock(const void* ptr);
    static GenericAck handle_ObserveChatId(const void* ptr);
    static GenericAck handle_ObserveAllChats(const void* ptr);
    static GenericAck handle_DeleteControllerById(const void* ptr);
    GenericAck handle_UploadFile(const void* ptr,
                                 TgBotSocket::PacketHeader::length_type len);
    UploadFileDryCallback handle_UploadFileDry(
        const void* ptr, TgBotSocket::PacketHeader::length_type len);

    // These have their own ack handlers
    bool handle_GetUptime(SocketConnContext ctx, const void* ptr);
    bool handle_DownloadFile(SocketConnContext ctx, const void* ptr);
};
