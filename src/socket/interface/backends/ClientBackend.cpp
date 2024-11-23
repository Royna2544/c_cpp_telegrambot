#include "ClientBackend.hpp"

#include <ConfigManager.hpp>
#include <TryParseStr.hpp>
#include <memory>
#include <string>
#include <boost/process/environment.hpp>
#include "Env.hpp"
#include "SocketContext.hpp"

bool SocketClientWrapper::connect(unsigned short defaultPort,
                                  std::filesystem::path defaultPath) {
    std::string addressString;
    boost::asio::ip::port_type port = 0;
    bool foundPort = false;
    Env env;
    std::string portStr;
    if (env[kPortEnvVar].assign(portStr)) {
        if (try_parse(portStr, &port)) {
            foundPort = true;
        }
    }
    if (!foundPort) {
        port = defaultPort;
    }
    LOG(INFO) << "Using port " << port;
    if (env[kIPv4EnvVar].assign(addressString)) {
        backend = std::make_shared<TgBotSocket::Context::TCP>(
            boost::asio::ip::tcp::v4(), port);
        LOG(INFO) << "Chose IPv4 with address " << addressString;
    } else if (env[kIPv6EnvVar].assign(addressString)) {
        backend = std::make_shared<TgBotSocket::Context::TCP>(
            boost::asio::ip::tcp::v6(), port);
        LOG(INFO) << "Chose IPv6 with address " << addressString;
    } else {
        addressString = defaultPath.string();
        backend = std::make_shared<TgBotSocket::Context::Local>();
        LOG(INFO) << "Chose Unix Local socket with path " << addressString;
    }
    return backend->connect({addressString, port});
}

const TgBotSocket::Context& SocketClientWrapper::chosen_interface() const {
    LOG_IF(ERROR, !backend) << "Client wrapper called without connection.";
    return *backend;
}
std::shared_ptr<TgBotSocket::Context> SocketClientWrapper::operator->() {
    LOG_IF(ERROR, !backend) << "Client wrapper called without connection.";
    return backend;
}