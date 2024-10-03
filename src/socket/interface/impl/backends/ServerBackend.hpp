#include <SocketBase.hpp>
#include <TgBotPPImplExports.h>

struct TgBotPPImpl_API SocketServerWrapper {
    explicit SocketServerWrapper();

    enum class BackendType { Ipv4, Ipv6, Local, Unknown };

    [[nodiscard]] std::shared_ptr<SocketInterfaceBase> getInternalInterface()
        const {
        return internalBackend;
    }
    [[nodiscard]] std::shared_ptr<SocketInterfaceBase> getExternalInterface()
        const {
        return externalBackend;
    }

   private:
    std::shared_ptr<SocketInterfaceBase> internalBackend;
    std::shared_ptr<SocketInterfaceBase> externalBackend;

    static BackendType fromString(const std::string& str);
    static std::shared_ptr<SocketInterfaceBase> getInterfaceForType(
        BackendType type);
    static std::shared_ptr<SocketInterfaceBase> getInterfaceForString(
        const std::string& str);
};