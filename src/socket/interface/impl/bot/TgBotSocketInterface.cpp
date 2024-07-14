#include "TgBotSocketInterface.hpp"

#include <ManagedThreads.hpp>
#include <memory>
#include <string>
#include <utility>

#include "../backends/ServerBackend.hpp"
#include "SocketBase.hpp"

using HandleState = SocketInterfaceTgBot::HandleState;

SocketInterfaceTgBot::SocketInterfaceTgBot(std::shared_ptr<SocketInterfaceBase> _interface)
    : interface(std::move(_interface)),
      TgBotSocketParser(_interface.get()) {}

void SocketInterfaceTgBot::doInitCall(Bot& bot) {
    auto mgr = ThreadManager::getInstance();
    SocketServerWrapper wrapper;
    std::vector<std::shared_ptr<SocketInterfaceTgBot>> threads;

    if (wrapper.getInternalInterface()) {
        threads.emplace_back(
            mgr->createController<ThreadManager::Usage::SOCKET_THREAD,
                                  SocketInterfaceTgBot>(
                 wrapper.getInternalInterface()));
    }
    if (wrapper.getExternalInterface()) {
        threads.emplace_back(
            mgr->createController<ThreadManager::Usage::SOCKET_EXTERNAL_THREAD,
                                  SocketInterfaceTgBot>(
                wrapper.getExternalInterface()));
    }
    for (auto& thr : threads) {
        thr->interface->options.port = SocketInterfaceBase::kTgBotHostPort;
        // TODO: This is only needed for AF_UNIX sockets
        thr->interface->options.address = SocketInterfaceBase::LocalHelper::getSocketPath().string();
        thr->run();
    }
}

void SocketInterfaceTgBot::runFunction() {
    setPreStopFunction([this](auto*) { interface->forceStopListening(); });
    interface->startListeningAsServer(
        [this](SocketConnContext ctx) { return onNewBuffer(std::move(ctx)); });
}
