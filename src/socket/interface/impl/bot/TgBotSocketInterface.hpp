#include <ManagedThreads.hpp>
#include <SocketBase.hpp>
#include <api/TgBotApi.hpp>
#include <functional>
#include <global_handlers/ChatObserver.hpp>
#include <global_handlers/SpamBlock.hpp>
#include <memory>

#include "ResourceManager.h"
#include "impl/SocketPosix.hpp"
#include "trivial_helpers/fruit_inject.hpp"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
#elif defined(WINDOWS_BUILD)
#include <ws2tcpip.h>
#endif

#include "TgBotSocketFileHelperNew.hpp"
#include "TgBotSocket_Export.hpp"

using TgBotSocket::callback::GenericAck;
using TgBotSocket::callback::UploadFileDryCallback;

struct SocketInterfaceTgBot : ManagedThreadRunnable {
    void handlePacket(SocketConnContext ctx, TgBotSocket::Packet pkt);

    void runFunction() override;

    APPLE_EXPLICIT_INJECT(SocketInterfaceTgBot(
        SocketInterfaceBase* _interface, TgBotApi::Ptr _api,
        ChatObserver* observer, SpamBlockBase* spamblock,
        SocketFile2DataHelper* helper, ResourceProvider* resource));

   private:
    SocketInterfaceBase* interface = nullptr;
    TgBotApi::Ptr api = nullptr;
    SocketFile2DataHelper* helper;
    ChatObserver* observer = nullptr;
    SpamBlockBase* spamblock = nullptr;
    ResourceProvider* resource = nullptr;

    std::chrono::system_clock::time_point startTp =
        std::chrono::system_clock::now();
    using command_handler_fn_t = std::function<void(
        socket_handle_t source, void* addr, socklen_t len, const void* data)>;

    // Command handlers
    GenericAck handle_WriteMsgToChatId(const void* ptr);
    GenericAck handle_SendFileToChatId(const void* ptr);
    GenericAck handle_CtrlSpamBlock(const void* ptr);
    GenericAck handle_ObserveChatId(const void* ptr);
    GenericAck handle_ObserveAllChats(const void* ptr);
    static GenericAck handle_DeleteControllerById(const void* ptr);
    GenericAck handle_UploadFile(const void* ptr,
                                 TgBotSocket::PacketHeader::length_type len);
    UploadFileDryCallback handle_UploadFileDry(
        const void* ptr, TgBotSocket::PacketHeader::length_type len);

    // These have their own ack handlers
    bool handle_GetUptime(SocketConnContext ctx, const void* ptr);
    bool handle_DownloadFile(SocketConnContext ctx, const void* ptr);
};
