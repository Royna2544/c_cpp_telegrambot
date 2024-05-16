#include "TgBotSocketInterface.hpp"

#include <SingleThreadCtrl.h>
#include <absl/log/log.h>

#include <new>
#include <optional>

#include "SharedMalloc.hpp"
#include "SocketBase.hpp"
#include "socket/TgBotSocket.h"

using HandleState = SocketInterfaceTgBot::HandleState;

SocketInterfaceTgBot::SocketInterfaceTgBot(
    Bot& bot, std::shared_ptr<SocketInterfaceBase> _interface)
    : interface(std::move(_interface)),
      BotClassBase(bot),
      TgBotSocketParser(_interface.get()) {}

void SocketInterfaceTgBot::doInitCall(Bot& bot) {
    auto mgr = SingleThreadCtrlManager::getInstance();
    struct SingleThreadCtrlManager::GetControllerRequest req {};
    req.usage = SingleThreadCtrlManager::USAGE_SOCKET_THREAD;
    auto inter = mgr->getController<SocketInterfaceTgBot>(
        req, std::ref(bot), std::make_shared<SocketInternalInterface>());
    req.usage = SingleThreadCtrlManager::USAGE_SOCKET_EXTERNAL_THREAD;
    auto exter = mgr->getController<SocketInterfaceTgBot>(
        req, std::ref(bot), std::make_shared<SocketExternalInterface>());
    for (const auto& intf : {inter, exter}) {
        intf->run();
    }
}

void SocketInterfaceTgBot::runFunction() {
    setPreStopFunction(
        [this](SingleThreadCtrl*) { interface->forceStopListening(); });
    interface->startListeningAsServer(
        [this](SocketConnContext ctx) { return onNewBuffer(ctx); });
}
