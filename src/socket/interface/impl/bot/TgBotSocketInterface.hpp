#include <TgBotPPImplExports.h>

#include <ManagedThreads.hpp>
#include <SocketBase.hpp>
#include <TgBotWrapper.hpp>
#include <functional>
#include <initcalls/Initcall.hpp>
#include <memory>

#include "TgBotPacketParser.hpp"
#include "TgBotSocketFileHelperNew.hpp"
#include "TgBotSocket_Export.hpp"

using TgBotSocket::callback::GenericAck;
using TgBotSocket::callback::UploadFileDryCallback;

struct TgBotPPImpl_API SocketInterfaceTgBot : ManagedThreadRunnable, InitCall {
    void doInitCall() override;
    const CStringLifetime getInitCallName() const override {
        return "Create sockets and setup";
    }

    void handlePacket(SocketConnContext ctx, TgBotSocket::Packet pkt);

    void runFunction() override;

    explicit SocketInterfaceTgBot(
        std::shared_ptr<SocketInterfaceBase> _interface,
        std::shared_ptr<TgBotApi> _api,
        std::shared_ptr<SocketFile2DataHelper> helper);

   private:
    std::shared_ptr<SocketInterfaceBase> interface = nullptr;
    std::shared_ptr<TgBotApi> api = nullptr;
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
