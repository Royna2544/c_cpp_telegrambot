#include <absl/log/log.h>

#include <SocketBase.hpp>

std::string SocketInterfaceBase::INetHelper::getExternalIP() {
    LOG(ERROR) << __func__ << " needs libcurl";
    return "Unknown";
}

size_t SocketInterfaceBase::INetHelper::externalIPCallback(void *contents,
                                                           size_t size,
                                                           size_t nmemb,
                                                           void *userp) {}