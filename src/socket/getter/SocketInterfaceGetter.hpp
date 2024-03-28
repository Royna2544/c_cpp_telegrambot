#pragma once

#include <SingleThreadCtrl.h>

#include "../SocketInterfaceBase.h"

struct SocketInterfaceGetter {
    enum class SocketNetworkType {
        TYPE_LOCAL_UNIX,
        TYPE_IPV4,
        TYPE_IPV6,
    };

    enum class SocketUsage {
        USAGE_INTERNAL =
            SingleThreadCtrlManager::ThreadUsage::USAGE_SOCKET_THREAD,
        USAGE_EXTERNAL =
            SingleThreadCtrlManager::ThreadUsage::USAGE_SOCKET_EXTERNAL_THREAD,
    };

    static constexpr SocketNetworkType typeForInternal =
        SocketNetworkType::TYPE_LOCAL_UNIX;
    static constexpr SocketNetworkType typeForExternal =
        SocketNetworkType::TYPE_IPV4;

    static std::shared_ptr<SocketInterfaceBase> get(
        const SocketNetworkType type, const SocketUsage usage);
    static std::shared_ptr<SocketInterfaceBase> getForClient();
};