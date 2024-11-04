#include "TgBotSocketInterface.hpp"

#include <ManagedThreads.hpp>
#include <api/TgBotApi.hpp>
#include <impl/backends/ServerBackend.hpp>
#include <impl/bot/TgBotPacketParser.hpp>
#include <utility>

SocketInterfaceTgBot::SocketInterfaceTgBot(SocketInterfaceBase* _interface,
                                           TgBotApi::Ptr _api,
                                           ChatObserver* observer,
                                           SpamBlockBase* spamblock,
                                           SocketFile2DataHelper* helper,
                                           ResourceProvider* resource)
    : _interface(_interface),
      api(_api),
      observer(observer),
      spamblock(spamblock),
      helper(helper),
      resource(resource) {}

void SocketInterfaceTgBot::runFunction() {
    _interface->options.port = SocketInterfaceBase::kTgBotHostPort;
    // TODO: This is only needed for AF_UNIX sockets
    _interface->options.address =
        SocketInterfaceBase::LocalHelper::getSocketPath().string();
    setPreStopFunction([this](auto*) { _interface->forceStopListening(); });
    _interface->startListeningAsServer([this](SocketConnContext ctx) {
        auto pkt = TgBotSocket::readPacket(_interface, ctx);
        if (pkt) {
            handlePacket(ctx, std::move(pkt.value()));
        }
        return true;
    });
}
