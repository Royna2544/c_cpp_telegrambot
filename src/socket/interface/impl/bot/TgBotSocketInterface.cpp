#include "TgBotSocketInterface.hpp"

#include <ManagedThreads.hpp>
#include <utility>

#include "SocketBase.hpp"

using HandleState = SocketInterfaceTgBot::HandleState;

SocketInterfaceTgBot::SocketInterfaceTgBot(
    Bot& bot, std::shared_ptr<SocketInterfaceBase> _interface)
    : interface(std::move(_interface)),
      BotClassBase(bot),
      TgBotSocketParser(_interface.get()) {}

void SocketInterfaceTgBot::doInitCall(Bot& bot) {
    auto mgr = ThreadManager::getInstance();
    auto inter = mgr->createController<ThreadManager::Usage::SOCKET_THREAD,
                                       SocketInterfaceTgBot>(
        std::ref(bot), std::make_shared<SocketInternalInterface>());
    auto exter =
        mgr->createController<ThreadManager::Usage::SOCKET_EXTERNAL_THREAD,
                              SocketInterfaceTgBot>(
            std::ref(bot), std::make_shared<SocketExternalInterface>());
    for (const auto& intf : {inter, exter}) {
        intf->run();
    }
}

void SocketInterfaceTgBot::runFunction() {
    setPreStopFunction([this](auto*) { interface->forceStopListening(); });
    interface->startListeningAsServer(
        [this](SocketConnContext ctx) { return onNewBuffer(std::move(ctx)); });
}
