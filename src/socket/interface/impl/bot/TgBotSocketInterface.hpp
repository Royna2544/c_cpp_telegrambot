#include <BotClassBase.h>
#include <SingleThreadCtrl.h>
#include <SocketConnectionHandler.h>
#include <socket/TgBotSocket.h>

#include <SocketBase.hpp>
#include <SocketData.hpp>
#include <initcalls/BotInitcall.hpp>
#include <memory>

#ifdef WINDOWS_BUILD
#include "impl/SocketWindows.hpp"

using SocketInternalInterface = SocketInterfaceWindowsLocal;
using SocketExternalInterface = SocketInterfaceWindowsIPv4;
#else // WINDOWS_BUILD
#include "impl/SocketPosix.hpp"

using SocketInternalInterface = SocketInterfaceUnixLocal;
using SocketExternalInterface = SocketInterfaceUnixIPv4;
#endif // WINDOWS_BUILD

struct SocketInterfaceTgBot : SingleThreadCtrlRunnable,
                              BotInitCall,
                              BotClassBase {
    enum class HandleState {
        Ok,      // OK: Continue parsing
        Ignore,  // Ignore: Parse was completed, skip to next buf
        Fail     // Fail: Parse failed, exit loop
    };

    void doInitCall(Bot &bot) override {
        auto mgr = SingleThreadCtrlManager::getInstance();
        struct SingleThreadCtrlManager::GetControllerRequest req {};
        req.usage = SingleThreadCtrlManager::USAGE_SOCKET_THREAD;
        auto inter = mgr->getController<SocketInterfaceTgBot>(
            req, std::ref(bot), std::make_shared<SocketInternalInterface>());
        req.usage = SingleThreadCtrlManager::USAGE_SOCKET_EXTERNAL_THREAD;
        auto exter = mgr->getController<SocketInterfaceTgBot>(
            req, std::ref(bot), std::make_shared<SocketExternalInterface>());
        for (const auto &intf : {inter, exter}) {
            intf->run();
        }
    }
    const CStringLifetime getInitCallName() const override {
        return "Create sockets and setup";
    }

    bool onNewBuffer(const Bot &bot, socket_handle_t cfd);

    /**
     * @brief Reads a packet header from the socket.
     *
     * @param socketData The SocketData object of data.
     * @param pkt The TgBotCommandPacket to fill.
     *
     * @return HandleState object containing the state.
     */
    [[nodiscard]] static HandleState handle_PacketHeader(
        std::optional<SocketData> &socketData,
        std::optional<TgBotCommandPacket> &pkt);

    /**
     * @brief Reads a packet from the socket.
     *
     * @param socketData The SocketData object of data.
     * @param pkt The TgBotCommandPacket to read the packet into.
     *
     * @return HandleState object containing the state.
     */
    [[nodiscard]] static HandleState handle_Packet(
        std::optional<SocketData> &socketData,
        std::optional<TgBotCommandPacket> &pkt);

    void runFunction() override;

    explicit SocketInterfaceTgBot(
        Bot &bot, std::shared_ptr<SocketInterfaceBase> _interface)
        : interface(_interface), BotClassBase(bot) {}
    // TODO Used by main.cpp
    SocketInterfaceTgBot(Bot &bot) : BotClassBase(bot) {}

   private:
    std::shared_ptr<SocketInterfaceBase> interface = nullptr;
};
