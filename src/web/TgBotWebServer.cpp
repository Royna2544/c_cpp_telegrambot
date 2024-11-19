#include <TgBotWebpage.hpp>
#include <stop_token>
#include <utility>

void TgBotWebServer::runFunction(const std::stop_token& /*token*/) {
    startServer();
}

void TgBotWebServer::onPreStop() {
    // Server will stop on its own after the callback is called.
    stopServer();
}
TgBotWebServer::TgBotWebServer(std::filesystem::path wwwResource,
                               int serverPort)
    : TgBotWebServerBase(serverPort, std::move(wwwResource)) {}
