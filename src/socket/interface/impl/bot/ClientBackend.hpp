#pragma once

#include <SocketBase.hpp>
#include <filesystem>
#include <memory>
#include <optional>

struct SocketClientWrapper {
    explicit SocketClientWrapper(
        std::optional<std::filesystem::path> localSocketPath = std::nullopt);
    [[nodiscard]] SocketInterfaceBase *getRawInterface() const {
        return backend.get();
    }
    SocketInterfaceBase *operator->() const { return getRawInterface(); }

   private:
    constexpr static std::string_view kIPv4EnvVar = "IPV4_ADDRESS";
    constexpr static std::string_view kIPv6EnvVar = "IPV6_ADDRESS";
    constexpr static std::string_view kPortEnvVar = "PORT_NUM";

    std::shared_ptr<SocketInterfaceBase> backend;
};
