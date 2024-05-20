#include <BotClassBase.h>
#include <SingleThreadCtrl.h>
#include <socket/TgBotSocket.h>

#include <SocketBase.hpp>
#include <functional>
#include <initcalls/BotInitcall.hpp>
#include <map>
#include <memory>
#include <utility>

#include "TgBotPacketParser.hpp"

using TgBotCommandData::GenericAck;

#ifdef WINDOWS_BUILD
#include "impl/SocketWindows.hpp"

using SocketInternalInterface = SocketInterfaceWindowsLocal;
using SocketExternalInterface = SocketInterfaceWindowsIPv4;
#else  // WINDOWS_BUILD
#include "impl/SocketPosix.hpp"

using SocketInternalInterface = SocketInterfaceUnixLocal;
using SocketExternalInterface = SocketInterfaceUnixIPv4;
#endif  // WINDOWS_BUILD

struct SocketInterfaceTgBot : SingleThreadCtrlRunnable,
                              BotInitCall,
                              BotClassBase,
                              TgBotSocketParser {
    void doInitCall(Bot& bot) override;
    const CStringLifetime getInitCallName() const override {
        return "Create sockets and setup";
    }

    void handle_CommandPacket(SocketConnContext ctx,
                              TgBotCommandPacket pkt) override;

    void runFunction() override;

    explicit SocketInterfaceTgBot(
        Bot& bot, std::shared_ptr<SocketInterfaceBase> _interface);

    // TODO Used by main.cpp    
    SocketInterfaceTgBot(Bot& bot) : BotClassBase(bot) {}

   private:
    std::shared_ptr<SocketInterfaceBase> interface = nullptr;
    decltype(std::chrono::system_clock::now()) startTp =
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
    static GenericAck handle_UploadFile(const void *ptr, TgBotCommandPacketHeader::length_type len);

    // These have their own ack handlers
    bool handle_GetUptime(SocketConnContext ctx, const void* ptr);
    bool handle_DownloadFile(SocketConnContext ctx, const void* ptr);
};
