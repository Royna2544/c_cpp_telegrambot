
#include <curl/curl.h>
#include "../SocketInterfaceBase.h"

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