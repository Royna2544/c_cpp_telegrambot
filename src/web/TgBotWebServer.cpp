#include <TgBotWebpage.hpp>

#include "libos/libfs.hpp"

void TgBotWebServer::runFunction() {
    onPreStop<TgBotWebServer>([](TgBotWebServer *thiz) {
        thiz->stopServer();  // Server will stop on its own after the callback
                             // is called.
    });
    startServer();
}

TgBotWebServer::TgBotWebServer(int serverPort)
    : TgBotWebServerBase(serverPort,
                         FS::getPathForType(FS::PathType::RESOURCES_WEBPAGE)) {}
