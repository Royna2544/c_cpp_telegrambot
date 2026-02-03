#include "CurlUtils.hpp"

#include <absl/log/log.h>
#include <curl/curl.h>
#include <fmt/format.h>

#include <StructF.hpp>
#include <libos/libsighandler.hpp>
#include <utility>

#include "BytesConversion.hpp"
#include "utils/libfs.hpp"

namespace {

static CURL* CURL_setup_common(const std::string_view url,
                               CurlUtils::CancelChecker& cancel_checker) {
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        LOG(ERROR) << "Cannot initialize curl";
        return nullptr;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    // Follow up to 5 redirects
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    // Enable 302 redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Set progress callback
    curl_easy_setopt(
        curl, CURLOPT_XFERINFOFUNCTION,
        +[](void* clientp, curl_off_t dltotal, curl_off_t dlnow,
            curl_off_t ultotal, curl_off_t ulnow) -> int {
            auto dlnowByte = MegaBytes(dlnow * boost::units::data::bytes);
            auto dltotalByte = MegaBytes(dltotal * boost::units::data::bytes);
            auto ulnowByte = MegaBytes(ulnow * boost::units::data::bytes);
            auto ultotalByte = MegaBytes(ultotal * boost::units::data::bytes);
            auto cancel_checker =
                static_cast<CurlUtils::CancelChecker*>(clientp);
            LOG_EVERY_N_SEC(INFO, 5) << fmt::format(
                "Download: {}MB/{}MB, Upload: {}MB/{}MB", dlnowByte.value(),
                dltotalByte.value(), ulnowByte.value(), ultotalByte.value());

            constexpr int CURL_STOP = 1;
            constexpr int CURL_CONTINUE = 0;

            assert(cancel_checker != nullptr);
            if (*cancel_checker == nullptr) {
                // No cancel checker, continue
                return CURL_CONTINUE;
            } else {
                // Call the cancel checker to see if we need to cancel
                bool rc = (*cancel_checker)();
                // If rc is true, we need to cancel.
                return rc ? CURL_STOP : CURL_CONTINUE;
            }
        });
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancel_checker);
    // Enble progress callbacks
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    return curl;
}

bool CURL_perform_common(CURL* curl) {
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "Cannot download file: " << curl_easy_strerror(res);
        return false;
    }
    return true;
}

}  // namespace

namespace CurlUtils {

bool download_file(const std::string_view url,
                   const std::filesystem::path& where,
                   CurlUtils::CancelChecker cancel_checker) {
    LOG(INFO) << "Downloading " << url << " to " << where;

    // Common CURL setup
    CURL* curl = CURL_setup_common(url, cancel_checker);
    if (curl == nullptr) {
        LOG(ERROR) << "Cannot setup curl";
        return false;
    }

    // Create directory if not exists
    if (!noex_fs::create_directories(where.parent_path())) {
        return false;
    }

    // Open the file for writing
    F file;
    if (!file.open(where, F::Mode::WriteBinary)) {
        LOG(ERROR) << "Cannot open file for writing";
        return false;
    }
    // Write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<FILE*>(file));

    // Execute it
    bool result = CURL_perform_common(curl);
    LOG_IF(INFO, result) << "Download succeeded, wrote to " << where;
    return result;
}

std::optional<std::string> download_memory(
    const std::string_view url, CurlUtils::CancelChecker cancel_checker,
    const std::string_view authkey) {
    std::string result;

    LOG(INFO) << "Downloading " << url << " to memory";
    if (!authkey.empty()) {
        LOG(INFO) << "Using authorization key for download";
    }

    // Common CURL setup
    CURL* curl = CURL_setup_common(url, cancel_checker);
    if (curl == nullptr) {
        LOG(ERROR) << "Cannot setup curl";
        return std::nullopt;
    }

    // Set authorization header if provided
    struct curl_slist* headers = nullptr;
    if (!authkey.empty()) {
        std::string auth_header =
            "Authorization: Bearer " + std::string(authkey);
        headers = curl_slist_append(headers, auth_header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    // Write callback
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION,
        +[](void* contents, size_t size, size_t nmemb, void* userp) {
            std::string s(static_cast<char*>(contents), size * nmemb);
            *static_cast<std::string*>(userp) += s;
            LOG_EVERY_N_SEC(INFO, 5)
                << fmt::format("Downloaded {} bytes so far",
                               static_cast<std::string*>(userp)->size());
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

    // Execute it
    bool exec_result = CURL_perform_common(curl);
    if (headers != nullptr) {
        curl_slist_free_all(headers);
    }
    LOG_IF(INFO, exec_result) << "Download succeeded";
    if (exec_result) {
        return result;
    } else {
        return std::nullopt;
    }
}

std::optional<std::string> send_json_get_reply(const std::string_view url,
                                               std::string json,
                                               const std::string_view authkey) {
    std::string result;

    LOG(INFO) << "Sending JSON to " << url;

    // Common CURL setup
    CurlUtils::CancelChecker cancel_checker = nullptr;
    CURL* curl = CURL_setup_common(url, cancel_checker);
    if (curl == nullptr) {
        LOG(ERROR) << "Cannot setup curl";
        return {};
    }

    // Set timeout for connection, lower than default
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!authkey.empty()) {
        std::string auth_header =
            "Authorization: Bearer " + std::string(authkey);
        headers = curl_slist_append(headers, auth_header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json.size());

    // Write callback
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION,
        +[](void* contents, size_t size, size_t nmemb, void* userp) {
            std::string s(static_cast<char*>(contents), size * nmemb);
            *static_cast<std::string*>(userp) += s;
            LOG_EVERY_N_SEC(INFO, 5)
                << fmt::format("Received {} bytes so far",
                               static_cast<std::string*>(userp)->size());
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

    // Execute it
    bool exec_result = CURL_perform_common(curl);
    curl_slist_free_all(headers);
    LOG_IF(INFO, exec_result) << "Request succeeded";
    if (exec_result)
        return result;
    else
        return {};
}

}  // namespace CurlUtils