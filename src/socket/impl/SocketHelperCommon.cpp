#include <libos/libfs.hpp>
#include <string>

#include "../SocketInterfaceBase.h"
#include "Logging.h"

bool SocketHelperCommon::_isAvailable(SocketInterfaceBase *it,
                                      const char *envVar) {
    char *addr = getenv(envVar);
    int portNum = SocketInterfaceBase::kTgBotHostPort;

    if (!addr) {
        LOG(LogLevel::DEBUG, "%s is not set, isAvailable false", envVar);
        return false;
    }
    it->setOptions(SocketInterfaceBase::Options::DESTINATION_ADDRESS, addr,
                   true);
    if (const char *port = getenv(kPortEnvVar); port != nullptr) {
        try {
            portNum = std::stoi(port);
            LOG(LogLevel::DEBUG, "%s is set", kPortEnvVar);
        } catch (...) {
            LOG(LogLevel::ERROR, "Illegal value for %s: %s", kPortEnvVar, port);
        }
        LOG(LogLevel::DEBUG, "Chosen port: %d", portNum);
    }
    it->setOptions(SocketInterfaceBase::Options::DESTINATION_PORT,
                   std::to_string(portNum), true);
    return true;
}

bool SocketHelperCommon::isAvailableIPv4(SocketInterfaceBase *it) {
    return _isAvailable(it, kIPv4EnvVar);
}

bool SocketHelperCommon::isAvailableIPv6(SocketInterfaceBase *it) {
    return _isAvailable(it, kIPv6EnvVar);
}

int SocketHelperCommon::getPortNumInet(SocketInterfaceBase *it) {
    return stoi(it->getOptions(SocketInterfaceBase::Options::DESTINATION_PORT));
}

bool SocketHelperCommon::isAvailableLocalSocket() {
    LOG(LogLevel::DEBUG, "Choosing local socket");
    return true;
}

bool SocketHelperCommon::canSocketBeClosedLocalSocket(SocketInterfaceBase *it) {
    bool socketValid = true;

    if (!FS::exists(it->getOptions(
            SocketInterfaceBase::Options::DESTINATION_ADDRESS))) {
        LOG(LogLevel::WARNING, "Socket file was deleted");
        socketValid = false;
    }
    return socketValid;
}

void SocketHelperCommon::cleanupServerSocketLocalSocket(
    SocketInterfaceBase *it) {
    std::filesystem::remove(
        it->getOptions(SocketInterfaceBase::Options::DESTINATION_ADDRESS));
}

#ifdef HAVE_CURL
#include <curl/curl.h>
#undef ERROR

void SocketHelperCommon::printExternalIPINet() {
    CURL *curl;
    CURLcode res;

    // Initialize libcurl
    curl = curl_easy_init();
    if (!curl) {
        LOG(LogLevel::ERROR, "Error initializing libcurl");
        return;
    }

    // Set the URL to ipinfo
    curl_easy_setopt(curl, CURLOPT_URL, "ipinfo.io/ip");

    // Set the callback function for handling the response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, externalIPCallback);

    // Perform the request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG(LogLevel::ERROR, "curl_easy_perform() failed: %s",
            curl_easy_strerror(res));
    }

    // Clean up
    curl_easy_cleanup(curl);
}

size_t SocketHelperCommon::externalIPCallback(void *contents, size_t size,
                                              size_t nmemb, void *userp) {
    std::string s;
    s.append((char *)contents, size * nmemb);
    LOG(LogLevel::DEBUG, "External IP addr: %s", s.c_str());
    return size * nmemb;
}
#else   // HAVE_CURL
void SocketHelperCommon::getExternalIPINet() {
    LOG(LogLevel::ERROR, "%s needs libcurl", __func__);
}
#endif  // HAVE_CURL