#include <absl/log/log.h>

#include <SocketBase.hpp>

void SocketInterfaceBase::INetHelper::printExternalIP() {
    LOG(ERROR) << __func__ << " needs libcurl";
}
size_t SocketInterfaceBase::INetHelper::externalIPCallback(void *contents,
                                                           size_t size,
                                                           size_t nmemb,
                                                           void *userp) {}