#pragma once

#include <SocketBase.hpp>
#include <memory>

struct SocketServerWrapper {
    explicit SocketServerWrapper(std::string config);

    enum class BackendType { Ipv4, Ipv6, Local, Unknown };

    [[nodiscard]] std::shared_ptr<SocketInterfaceBase> getInternalInterface()
        const {
        return getInterfaceForType(internal);
    }
    [[nodiscard]] std::shared_ptr<SocketInterfaceBase> getExternalInterface()
        const {
        return getInterfaceForType(external);
    }

   private:
    BackendType internal;
    BackendType external;

    static BackendType fromString(const std::string_view str);
    static std::shared_ptr<SocketInterfaceBase> getInterfaceForType(
        BackendType type);
};