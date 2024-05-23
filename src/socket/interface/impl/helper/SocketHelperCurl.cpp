
#include <absl/log/log.h>
#include <curl/curl.h>

#include <SocketBase.hpp>
#include <future>

std::string SocketInterfaceBase::INetHelper::getExternalIP() {
    CURL *curl = nullptr;
    CURLcode res = {};
    std::promise<std::string> addr_promise;
    auto future = addr_promise.get_future();

    // Initialize libcurl
    curl = curl_easy_init();
    if (curl == nullptr) {
        LOG(ERROR) << "Error initializing libcurl";
        return "[libcurl initialization failed]";
    }

    // Set the URL to ipinfo
    curl_easy_setopt(curl, CURLOPT_URL, "ipinfo.io/ip");

    // Set the callback function for handling the response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, externalIPCallback);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &addr_promise);

    // Perform the request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "curl_easy_perform() failed: " << curl_easy_strerror(res);
    }

    // Clean up
    curl_easy_cleanup(curl);

    return future.get();
}

size_t SocketInterfaceBase::INetHelper::externalIPCallback(void *contents,
                                                           size_t size,
                                                           size_t nmemb,
                                                           void *userp) {
    std::string s;
    auto *addr_future = static_cast<std::promise<std::string> *>(userp);

    s.append(static_cast<char *>(contents), size * nmemb);
    LOG(INFO) << "External IP addr: " << s;
    addr_future->set_value(s);
    return size * nmemb;
}