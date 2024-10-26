#include "TgBotSocketInterface.hpp"

#include <ManagedThreads.hpp>
#include <api/TgBotApi.hpp>
#include <impl/backends/ServerBackend.hpp>
#include <impl/bot/TgBotPacketParser.hpp>
#include <memory>
#include <utility>

SocketInterfaceTgBot::SocketInterfaceTgBot(SocketInterfaceBase* _interface,
                                           TgBotApi::Ptr _api,
                                           ChatObserver* observer,
                                           SpamBlockBase* spamblock,
                                           SocketFile2DataHelper* helper,
                                           ResourceManager* resource)
    : interface(_interface),
      api(_api),
      observer(observer),
      spamblock(spamblock),
      helper(helper),
      resource(resource) {}

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
        }
        return true;
    });
}
