#include "TgBotSocketInterface.hpp"

#include <ManagedThreads.hpp>
#include <TgBotWrapper.hpp>
#include <impl/backends/ServerBackend.hpp>
#include <impl/bot/TgBotPacketParser.hpp>
#include <memory>
#include <utility>

SocketInterfaceTgBot::SocketInterfaceTgBot(
    std::shared_ptr<SocketInterfaceBase> _interface,
    std::shared_ptr<TgBotApi> _api,
    std::shared_ptr<SocketFile2DataHelper> helper)
    : interface(std::move(_interface)),
      api(std::move(_api)),
      helper(std::move(helper)) {}

void SocketInterfaceTgBot::runFunction() {
    interface->options.port = SocketInterfaceBase::kTgBotHostPort;
    // TODO: This is only needed for AF_UNIX sockets
    interface->options.address =
        SocketInterfaceBase::LocalHelper::getSocketPath().string();
    setPreStopFunction([this](auto*) { interface->forceStopListening(); });
    interface->startListeningAsServer([this](SocketConnContext ctx) {
        auto pkt = TgBotSocket::readPacket(interface, ctx);
        if (pkt) {
            handlePacket(ctx, std::move(pkt.value()));
            return true;
        }
        return false;
    });
}
