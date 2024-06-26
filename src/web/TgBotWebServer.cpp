#include <TgBotWebpage.hpp>

#include "ManagedThreads.hpp"
#include "libos/libfs.hpp"

void TgBotWebServer::runFunction() { startServer(); }

const CStringLifetime TgBotWebServer::getInitCallName() const {
    return "Start webserver";
}

void TgBotWebServer::doInitCall() {
    setPreStopFunction([this](auto *) { stopServer(); });
    run();
}

TgBotWebServer::TgBotWebServer(int serverPort)
    : TgBotWebServerBase(serverPort,
                         FS::getPathForType(FS::PathType::RESOURCES_WEBPAGE)) {}
