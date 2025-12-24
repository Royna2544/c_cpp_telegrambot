#include "ClientBackend.hpp"

#include <TryParseStr.hpp>
#include <absl/log/log.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

#include "Env.hpp"

namespace TgBotSocket {

namespace {

/**
 * @brief Parse port from environment or use default
 */
::std::uint16_t getPortFromEnv(const Env& env, ::std::uint16_t defaultPort) {
    ::std::string portStr;
    ::std::uint16_t port = 0;

    if (env[SocketClientWrapper::kPortEnvVar].assign(portStr)) {
        if (try_parse(portStr, &port)) {
            LOG(INFO) << "Using port from environment: " << port;
            return port;
        }
        LOG(WARNING) << "Invalid port in environment, using default";
    }

    LOG(INFO) << "Using default port: " << defaultPort;
    return defaultPort;
}

/**
 * @brief Log connection attempt details
 */
void logConnectionAttempt(const ConnectionConfig& config) {
    switch (config.type) {
        case ConnectionType::IPv4:
            LOG(INFO) << "Connecting to IPv4: " << config.address 
                      << ":" << config.port;
            break;
        case ConnectionType::IPv6:
            LOG(INFO) << "Connecting to IPv6: [" << config.address 
                      << "]:" << config.port;
            break;
        case ConnectionType::UnixLocal:
            LOG(INFO) << "Connecting to Unix socket: " << config.path;
            break;
        case ConnectionType::UDP_IPv4:
            LOG(INFO) << "Connecting to UDP IPv4: " << config.address 
                      << ":" << config.port;
            break;
        case ConnectionType::UDP_IPv6:
            LOG(INFO) << "Connecting to UDP IPv6: [" << config.address 
                      << "]:" << config.port;
            break;
    }
}

}  // namespace

::std::optional<ConnectionConfig> SocketClientWrapper::detectFromEnvironment(
    ::std::uint16_t defaultPort, 
    const ::std::filesystem::path& defaultPath) const {
    
    Env env;
    
    // Log environment variables for debugging
    DLOG(INFO) << "Environment variables:";
    DLOG(INFO) << "  IPv4_ADDRESS: " << env[kIPv4EnvVar];
    DLOG(INFO) << "  IPv6_ADDRESS: " << env[kIPv6EnvVar];
    DLOG(INFO) << "  PORT_NUM: " << env[kPortEnvVar];
    DLOG(INFO) << "  USE_UDP: " << env[kUseUDPEnvVar];

    const ::std::uint16_t port = getPortFromEnv(env, defaultPort);

    ConnectionConfig config;
    config.port = port;

    // Check for UDP flag
    ::std::string useUDP;
    bool isUDP = env[kUseUDPEnvVar].assign(useUDP) && 
                 (useUDP == "1" || useUDP == "true" || useUDP == "yes");

    // Try IPv4 (TCP or UDP based on USE_UDP flag)
    if (env[kIPv4EnvVar].assign(config.address)) {
        config.type = isUDP ? ConnectionType::UDP_IPv4 : ConnectionType::IPv4;
        return config;
    }

    // Try IPv6 (TCP or UDP based on USE_UDP flag)
    if (env[kIPv6EnvVar].assign(config.address)) {
        config.type = isUDP ? ConnectionType::UDP_IPv6 : ConnectionType::IPv6;
        return config;
    }

    // Fallback to Unix socket
    if (!defaultPath.empty()) {
        config.type = ConnectionType::UnixLocal;
        config.path = defaultPath;
        config.address = defaultPath.string();
        return config;
    }

    LOG(ERROR) << "No connection method available (no env vars or default path)";
    return ::std::nullopt;
}

::std::shared_ptr<Context> SocketClientWrapper::createBackend(
    const ConnectionConfig& config) {
    
    ::std::shared_ptr<Context> ctx;

    switch (config.type) {
        case ConnectionType::IPv4:
            ctx = ::std::make_shared<Context::TCP>(
                boost::asio::ip::tcp::v4(), config.port);
            break;

        case ConnectionType::IPv6:
            ctx = ::std::make_shared<Context::TCP>(
                boost::asio::ip::tcp::v6(), config.port);
            break;

        case ConnectionType::UnixLocal:
            ctx = ::std::make_shared<Context::Local>();
            break;

        case ConnectionType::UDP_IPv4:
            ctx = ::std::make_shared<Context::UDP>(
                boost::asio::ip::udp::v4(), config.port);
            break;

        case ConnectionType::UDP_IPv6:
            ctx = ::std::make_shared<Context::UDP>(
                boost::asio::ip::udp::v6(), config.port);
            break;
    }

    if (ctx) {
        // Configure connection timeout
        ctx->options.connect_timeout = ::std::chrono::seconds(5);
    }

    return ctx;
}

bool SocketClientWrapper::connect(::std::uint16_t defaultPort,
                                  ::std::filesystem::path defaultPath) {
    auto config = detectFromEnvironment(defaultPort, defaultPath);
    if (!config) {
        return false;
    }

    return connect(*config);
}

bool SocketClientWrapper::connect(const ConnectionConfig& config) {
    logConnectionAttempt(config);

    backend = createBackend(config);
    if (!backend) {
        LOG(ERROR) << "Failed to create backend context";
        return false;
    }

    const bool success = backend->connect({config.address, config.port});
    if (!success) {
        LOG(ERROR) << "Connection failed";
        backend.reset();
    }

    return success;
}

const Context& SocketClientWrapper::chosen_interface() const {
    if (!backend) {
        LOG(ERROR) << "Client wrapper accessed without active connection";
        throw ::std::runtime_error("No active connection");
    }
    return *backend;
}

::std::shared_ptr<Context> SocketClientWrapper::operator->() {
    if (!backend) {
        LOG(ERROR) << "Client wrapper accessed without active connection";
        throw ::std::runtime_error("No active connection");
    }
    return backend;
}

}  // namespace TgBotSocket
