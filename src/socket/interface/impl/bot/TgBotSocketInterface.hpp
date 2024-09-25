#include <ManagedThreads.hpp>
#include <SocketBase.hpp>
#include <functional>
#include <initcalls/Initcall.hpp>
#include <TgBotWrapper.hpp>
#include <memory>

#include "TgBotPacketParser.hpp"
#include "TgBotSocket_Export.hpp"

#ifdef WINDOWS_BUILD
#include "impl/SocketWindows.hpp"
#else  // WINDOWS_BUILD
#include "impl/SocketPosix.hpp"
#endif  // WINDOWS_BUILD

#include <TgBotPPImplExports.h>

using TgBotSocket::callback::GenericAck;
using TgBotSocket::callback::UploadFileDryCallback;

struct TgBotPPImpl_API SocketInterfaceTgBot : ManagedThreadRunnable,
                                              InitCall,
                                              TgBotSocketParser {
    void doInitCall() override;
    const CStringLifetime getInitCallName() const override {
        return "Create sockets and setup";
    }

    void handle_CommandPacket(SocketConnContext ctx,
                              TgBotSocket::Packet pkt) override;

    void runFunction() override;

    explicit SocketInterfaceTgBot(
        std::shared_ptr<SocketInterfaceBase> _interface, std::shared_ptr<TgBotApi> _api);

   private:
    std::shared_ptr<SocketInterfaceBase> interface = nullptr;
    std::shared_ptr<TgBotApi> api = nullptr;
    
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
    static GenericAck handle_UploadFile(
        const void* ptr, TgBotSocket::PacketHeader::length_type len);
    static UploadFileDryCallback handle_UploadFileDry(
        const void* ptr, TgBotSocket::PacketHeader::length_type len);

    // These have their own ack handlers
    bool handle_GetUptime(SocketConnContext ctx, const void* ptr);
    bool handle_DownloadFile(SocketConnContext ctx, const void* ptr);
};
