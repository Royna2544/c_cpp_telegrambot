#include "SocketInterface.hpp"

#include <ManagedThreads.hpp>
#include <api/TgBotApi.hpp>
#include <string_view>
#include <utility>

#include "../CommandMap.hpp"
#include "ApiDef.hpp"
#include "PacketParser.hpp"

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
      resource(resource) {
    _interface->options.io_timeout = std::chrono::seconds(5);
}

void SocketInterfaceTgBot::runFunction(const std::stop_token& token) {
    bool ret = _interface->listen(
        [this, token](const TgBotSocket::Context& ctx) {
            while (!token.stop_requested()) {
                std::optional<TgBotSocket::Packet> pkt;
                pkt = readPacket(ctx);
                if (!pkt) {
                    break;
                }
                if (pkt->header.cmd == TgBotSocket::Command::CMD_OPEN_SESSION) {
                    handle_OpenSession(ctx);
                    continue;
                } else if (!verifyHeader(*pkt)) {
                    DLOG(INFO) << "Aborting connection";
                    handle_CloseSession(pkt->header.session_token);
                    break;
                } else if (pkt->header.cmd ==
                           TgBotSocket::Command::CMD_CLOSE_SESSION) {
                    handle_CloseSession(pkt->header.session_token);
                    break;
                } else {
                    handlePacket(ctx, std::move(pkt.value()));
                }
            }
        },
        true);
    if (!ret) {
        LOG(ERROR) << "Failed to start listening on socket";
    }
}

void SocketInterfaceTgBot::onPreStop() { _interface->abortConnections(); }
