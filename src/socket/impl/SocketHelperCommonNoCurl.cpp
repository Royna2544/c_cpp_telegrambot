#include "../SocketInterfaceBase.h"

void SocketHelperCommon::printExternalIPINet() {
    LOG(LogLevel::ERROR, "%s needs libcurl", __func__);
}
size_t SocketHelperCommon::externalIPCallback(void *contents, size_t size,
                                              size_t nmemb, void *userp) {}