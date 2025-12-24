#pragma once

#include <SocketContext.hpp>
#include <ApiDef.hpp>
#include <filesystem>
#include <memory>
#include <optional>

namespace TgBotSocket {

/**
 * @brief Connection type for socket client
 */
enum class ConnectionType {
    IPv4,
    IPv6,
    UnixLocal,
    UDP_IPv4,
    UDP_IPv6
};

/**
 * @brief Connection configuration
 */
struct ConnectionConfig {
    ConnectionType type;
    ::std::string address;
    ::std::uint16_t port;
    ::std::filesystem::path path;
};

/**
 * @brief Socket client wrapper with connection management
 */
class SOCKET_EXPORT SocketClientWrapper {
public:
    SocketClientWrapper() = default;

    /**
     * @brief Connect to server with default fallback
     * @param defaultPort Default port if not specified in environment
     * @param defaultPath Default Unix socket path if not specified
     * @return true if connection successful
     */
    bool connect(::std::uint16_t defaultPort, ::std::filesystem::path defaultPath);

    /**
     * @brief Connect with explicit configuration
     * @param config Connection configuration
     * @return true if connection successful
     */
    bool connect(const ConnectionConfig& config);

    /**
     * @brief Get the active connection interface
     */
    [[nodiscard]] const Context& chosen_interface() const;

    /**
     * @brief Access the backend context
     */
    ::std::shared_ptr<Context> operator->();

    /**
     * @brief Check if currently connected
     */
    [[nodiscard]] bool is_connected() const noexcept {
        return backend != nullptr;
    }

    constexpr static ::std::string_view kIPv4EnvVar = "IPV4_ADDRESS";
    constexpr static ::std::string_view kIPv6EnvVar = "IPV6_ADDRESS";
    constexpr static ::std::string_view kPortEnvVar = "PORT_NUM";
    constexpr static ::std::string_view kUseUDPEnvVar = "USE_UDP";

private:
    /**
     * @brief Detect connection configuration from environment
     */
    ::std::optional<ConnectionConfig> detectFromEnvironment(
        ::std::uint16_t defaultPort,
        const ::std::filesystem::path& defaultPath) const;

    /**
     * @brief Create backend context based on config
     */
    ::std::shared_ptr<Context> createBackend(const ConnectionConfig& config);

    ::std::shared_ptr<Context> backend;
};

}  // namespace TgBotSocket
