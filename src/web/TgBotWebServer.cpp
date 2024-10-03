#include <TgBotWebpage.hpp>

#include "libos/libfs.hpp"

void TgBotWebServer::runFunction() { startServer(); }

TgBotWebServer::TgBotWebServer(int serverPort)
    : TgBotWebServerBase(serverPort,
                         FS::getPathForType(FS::PathType::RESOURCES_WEBPAGE)) {}
