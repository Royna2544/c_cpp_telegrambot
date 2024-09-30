#include "TgBotSocketInterface.hpp"

#include <ManagedThreads.hpp>
#include <memory>
#include <utility>

#include "../backends/ServerBackend.hpp"
#include "SocketBase.hpp"
#include "TgBotWrapper.hpp"
#include "impl/bot/TgBotPacketParser.hpp"

SocketInterfaceTgBot::SocketInterfaceTgBot(
    std::shared_ptr<SocketInterfaceBase> _interface,
    std::shared_ptr<TgBotApi> _api,
    std::shared_ptr<SocketFile2DataHelper> helper)
    : interface(std::move(_interface)),
      api(std::move(_api)),
      helper(std::move(helper)) {}

void SocketInterfaceTgBot::doInitCall() {
    auto mgr = ThreadManager::getInstance();
    SocketServerWrapper wrapper;
    std::vector<std::shared_ptr<SocketInterfaceTgBot>> threads;

    if (wrapper.getInternalInterface()) {
        threads.emplace_back(
            mgr->createController<ThreadManager::Usage::SOCKET_THREAD,
                                  SocketInterfaceTgBot>(
                wrapper.getInternalInterface(), api, helper));
    }
    if (wrapper.getExternalInterface()) {
        threads.emplace_back(
            mgr->createController<ThreadManager::Usage::SOCKET_EXTERNAL_THREAD,
                                  SocketInterfaceTgBot>(
                wrapper.getExternalInterface(), api, helper));
    }
    for (auto& thr : threads) {
        thr->interface->options.port = SocketInterfaceBase::kTgBotHostPort;
        // TODO: This is only needed for AF_UNIX sockets
        thr->interface->options.address =
            SocketInterfaceBase::LocalHelper::getSocketPath().string();
        thr->run();
    }
}

void SocketInterfaceTgBot::runFunction() {
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
