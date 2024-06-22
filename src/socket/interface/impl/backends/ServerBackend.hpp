#include <SocketBase.hpp>

struct SocketServerWrapper {
    explicit SocketServerWrapper();

    // TODO: Use this
    enum class BackendType { Ipv4, Ipv6, Local };

    [[nodiscard]] std::shared_ptr<SocketInterfaceBase> getInternalInterface()
        const {
        return internalBackend;
    }
    [[nodiscard]] std::shared_ptr<SocketInterfaceBase> getExternalInterface()
        const {
        return externalBackend;
    }
    static std::shared_ptr<SocketInterfaceBase> getInterfaceForName(
        const std::string &name);

   private:
    std::shared_ptr<SocketInterfaceBase> internalBackend;
    std::shared_ptr<SocketInterfaceBase> externalBackend;
};