#include "TgBotSocketInterface.hpp"

#include <ManagedThreads.hpp>
#include <api/TgBotApi.hpp>
#include <utility>
#include "TgBotPacketParser.hpp"

SocketInterfaceTgBot::SocketInterfaceTgBot(TgBotSocket::Context* _interface,
                                           TgBotApi::Ptr _api,
                                           ChatObserver* observer,
                                           SpamBlockBase* spamblock,
                                           SocketFile2DataHelper* helper,
                                           ResourceProvider* resource)
    : _interface(_interface),
      api(_api),
      helper(helper),
      observer(observer),
      spamblock(spamblock),
      resource(resource) {}

void SocketInterfaceTgBot::runFunction(const std::stop_token& token) {
    bool ret = _interface->listen([this](const TgBotSocket::Context& ctx) {
        auto pkt = TgBotSocket::readPacket(ctx);
        if (pkt) {
            handlePacket(ctx, std::move(pkt.value()));
        }
    });
    if (!ret) {
        LOG(ERROR) << "Failed to start listening on socket";
    }
}

void SocketInterfaceTgBot::onPreStop() { _interface->abortConnections(); }
