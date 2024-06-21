#include <TgBotWebpage.hpp>

#include "ManagedThreads.hpp"
#include "libos/libfs.hpp"

void TgBotWebServer::runFunction() { startServer(); }

const CStringLifetime TgBotWebServer::getInitCallName() const {
    return "Start webserver";
}

void TgBotWebServer::doInitCall() {
    auto webThr = ThreadManager::getInstance()
                      ->createController<ThreadManager::Usage::WEBSERVER_THREAD,
                                         TgBotWebServer>(8080);
    webThr->setPreStopFunction([webThr](auto *) { webThr->stopServer(); });
    webThr->run();
}

TgBotWebServer::TgBotWebServer(int serverPort)
    : TgBotWebServerBase(serverPort,
                         FS::getPathForType(FS::PathType::RESOURCES_WEBPAGE)) {}
