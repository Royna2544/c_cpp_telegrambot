
#include <curl/curl.h>
#include <absl/log/log.h>

#include <SocketBase.hpp>

void SocketInterfaceBase::INetHelper::printExternalIP() {
    CURL *curl = nullptr;
    CURLcode res = {};

    // Initialize libcurl
    curl = curl_easy_init();
    if (curl == nullptr) {
        LOG(ERROR) << "Error initializing libcurl";
        return;
    }

    // Set the URL to ipinfo
    curl_easy_setopt(curl, CURLOPT_URL, "ipinfo.io/ip");

    // Set the callback function for handling the response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, externalIPCallback);

    // Perform the request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "curl_easy_perform() failed: " << curl_easy_strerror(res);
    }

    // Clean up
    curl_easy_cleanup(curl);
}

size_t SocketInterfaceBase::INetHelper::externalIPCallback(void *contents, size_t size,
                                              size_t nmemb, void */*userp*/) {
    std::string s;
    s.append(static_cast<char *>(contents), size * nmemb);

    LOG(INFO) << "External IP addr: " << s;
    return size * nmemb;
}