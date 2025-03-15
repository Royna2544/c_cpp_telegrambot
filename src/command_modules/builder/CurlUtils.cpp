#include "CurlUtils.hpp"

#include <absl/log/log.h>
#include <curl/curl.h>
#include <fmt/format.h>

#include <StructF.hpp>

#include "BytesConversion.hpp"
#include "utils/libfs.hpp"

bool CURL_download_file(const std::string_view url,
                        const std::filesystem::path& where) {
    std::string result;
    CURL* curl = nullptr;
    CURLcode res{};

    LOG(INFO) << "Downloading " << url << " to " << where;
    curl = curl_easy_init();
    if (curl == nullptr) {
        LOG(ERROR) << "Cannot initialize curl";
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    // Follow up to 5 redirects
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    // Enable 302 redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (createDirectory(where.parent_path())) {
        LOG(ERROR) << "Cannot create directory: " << where.parent_path();
        return false;
    }
    F file;
    if (!file.open(where, F::Mode::WriteBinary)) {
        LOG(ERROR) << "Cannot open file for writing";
        return false;
    }
    // Write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<FILE*>(file));

    // Set progress callback
    curl_easy_setopt(
        curl, CURLOPT_XFERINFOFUNCTION,
        +[](void* /*clientp*/, curl_off_t /*dltotal*/, curl_off_t dlnow,
            curl_off_t /*ultotal*/, curl_off_t ulnow) -> int {
            auto dlnowByte = MegaBytes(dlnow * boost::units::data::bytes);
            auto dltotalByte = MegaBytes(ulnow * boost::units::data::bytes);
            LOG_EVERY_N_SEC(INFO, 5) << fmt::format(
                "Download: {}MB/{}MB", dlnowByte.value(), dltotalByte.value());
            return 1;  // Continue downloading
        });
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, nullptr);
    // Execute it
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << curl_easy_strerror(res);
        LOG(ERROR) << "Cannot download file";
        return false;
    }
    LOG(INFO) << "Wrote downloaded file to " << where;
    return true;
}

std::optional<std::string> CURL_download_memory(const std::string_view url) {
    CURL* curl = nullptr;
    CURLcode res{};
    std::string result;

    LOG(INFO) << "Downloading " << url << " to memory";
    curl = curl_easy_init();
    if (curl == nullptr) {
        LOG(ERROR) << "Cannot initialize curl";
        return std::nullopt;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION,
        +[](void* contents, size_t size, size_t nmemb, void* userp) {
            std::string s(static_cast<char*>(contents), size * nmemb);
            *static_cast<std::string*>(userp) += s;
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << curl_easy_strerror(res);
        LOG(ERROR) << "Cannot download from link";
        return std::nullopt;
    }
    return result;
}
