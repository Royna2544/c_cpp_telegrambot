#include "TgBotSocketInterface.hpp"

#include <ManagedThreads.hpp>
#include <memory>
#include <string>
#include <utility>

#include "../backends/ServerBackend.hpp"
#include "SocketBase.hpp"

using HandleState = SocketInterfaceTgBot::HandleState;

SocketInterfaceTgBot::SocketInterfaceTgBot(
    Bot& bot, std::shared_ptr<SocketInterfaceBase> _interface)
    : interface(std::move(_interface)),
      BotClassBase(bot),
      TgBotSocketParser(_interface.get()) {}

void SocketInterfaceTgBot::doInitCall(Bot& bot) {
    auto mgr = ThreadManager::getInstance();
    SocketServerWrapper wrapper;
    std::vector<std::shared_ptr<SocketInterfaceTgBot>> threads;

    if (wrapper.getInternalInterface()) {
        threads.emplace_back(
            mgr->createController<ThreadManager::Usage::SOCKET_THREAD,
                                  SocketInterfaceTgBot>(
                std::ref(bot), wrapper.getInternalInterface()));
    }
    if (wrapper.getExternalInterface()) {
        threads.emplace_back(
            mgr->createController<ThreadManager::Usage::SOCKET_EXTERNAL_THREAD,
                                  SocketInterfaceTgBot>(
                std::ref(bot), wrapper.getExternalInterface()));
    }
    for (auto& thr : threads) {
        thr->interface->setOptions(
            SocketInterfaceBase::Options::DESTINATION_PORT,
            std::to_string(SocketInterfaceBase::kTgBotHostPort));
        thr->run();
    }
}

void SocketInterfaceTgBot::runFunction() {
    setPreStopFunction([this](auto*) { interface->forceStopListening(); });
    interface->startListeningAsServer(
        [this](SocketConnContext ctx) { return onNewBuffer(std::move(ctx)); });
}
