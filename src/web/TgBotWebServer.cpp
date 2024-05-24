#include <TgBotWebpage.hpp>

#include "SingleThreadCtrl.h"
#include "libos/libfs.hpp"

void TgBotWebServer::runFunction() { startServer(); }

const CStringLifetime TgBotWebServer::getInitCallName() const {
    return "Start webserver";
}

void TgBotWebServer::doInitCall() {
    static SingleThreadCtrlManager::GetControllerRequest request{};
    request.usage = SingleThreadCtrlManager::USAGE_WEBSERVER_THREAD;
    auto webThr =
        SingleThreadCtrlManager::getInstance()->getController<TgBotWebServer>(
            request, 8080);
    webThr->setPreStopFunction([webThr](auto *) { webThr->stopServer(); });
    webThr->run();
}

TgBotWebServer::TgBotWebServer(int serverPort)
    : TgBotWebServerBase(
          serverPort, FS::getPathForType(FS::PathType::RESOURCES_WEBPAGE)) {}
