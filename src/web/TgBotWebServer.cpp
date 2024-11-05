#include <TgBotWebpage.hpp>
#include <stop_token>

#include "libfs.hpp"

void TgBotWebServer::runFunction(const std::stop_token& token) {
    startServer();
}

void TgBotWebServer::onPreStop() {
    // Server will stop on its own after the callback is called.
    stopServer();
}
TgBotWebServer::TgBotWebServer(int serverPort)
    : TgBotWebServerBase(serverPort,
                         FS::getPath(FS::PathType::RESOURCES_WEBPAGE)) {}
