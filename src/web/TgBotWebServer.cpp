#include <absl/log/log.h>

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
                               int serverPort, std::string grpcServerAddr)
    : TgBotWebServerBase(serverPort, std::move(wwwResource),
                         std::move(grpcServerAddr)) {}
