#pragma once

#include <SocketContext.hpp>
#include <TgBotSocket_Export.hpp>
#include <filesystem>
#include <memory>

struct Socket_API SocketClientWrapper {
    SocketClientWrapper() = default;
    bool connect(unsigned short defaultPort, std::filesystem::path defaultPath);
    [[nodiscard]] const TgBotSocket::Context& chosen_interface() const;
    std::shared_ptr<TgBotSocket::Context> operator->();

   private:
    constexpr static std::string_view kIPv4EnvVar = "IPV4_ADDRESS";
    constexpr static std::string_view kIPv6EnvVar = "IPV6_ADDRESS";
    constexpr static std::string_view kPortEnvVar = "PORT_NUM";

    std::shared_ptr<TgBotSocket::Context> backend;
};
