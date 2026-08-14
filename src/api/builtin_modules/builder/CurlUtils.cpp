#include "CurlUtils.hpp"

#include <absl/log/log.h>
#include <curl/curl.h>
#include <fmt/format.h>

#include <StructF.hpp>
#include <cstdint>
#include <libos/libsighandler.hpp>
#include <string>
#include <utility>
#include <vector>

#include "utils/libfs.hpp"

namespace {

static CURL* CURL_setup_common(const std::string_view url,
                               CurlUtils::CancelChecker& cancel_checker,
                               bool timeout = true) {
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        LOG(ERROR) << "Cannot initialize curl";
        return nullptr;
    }

    // CURLOPT_URL needs a NUL-terminated C string; string_view::data() is not
    // guaranteed terminated. curl copies the URL at setopt time, so a local
    // string is sufficient.
    const std::string url_str(url);
    curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
    // Follow up to 5 redirects
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    // Enable 302 redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    // Curl handles timeouts without process-wide signals. This is required
    // when requests run concurrently on command worker threads.
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    constexpr auto megabyte = 1024L * 1024L;

    if (timeout) {
        // Set connect timeout: 30s
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        // Set overall timeout: 300s
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
        // Set low speed limit: 1KB/s
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
        // Set low speed time: 30s
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    }

    // Set progress callback
    curl_easy_setopt(
        curl, CURLOPT_XFERINFOFUNCTION,
        +[](void* clientp, curl_off_t dltotal, curl_off_t dlnow,
            curl_off_t ultotal, curl_off_t ulnow) -> int {
            auto cancel_checker =
                static_cast<CurlUtils::CancelChecker*>(clientp);
            LOG_EVERY_N_SEC(INFO, 5) << fmt::format(
                "Download: {}MB/{}MB, Upload: {}MB/{}MB", dlnow / megabyte,
                dltotal / megabyte, ulnow / megabyte, ultotal / megabyte);

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

void CURL_apply_interactive_timeouts(CURL* curl) {
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,
                     CurlUtils::kInteractiveConnectTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,
                     CurlUtils::kInteractiveTotalTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,
                     CurlUtils::kInteractiveIdleTimeoutSeconds);
}

bool CURL_perform_common(CURL* curl) {
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        LOG(ERROR) << "Cannot download file: " << curl_easy_strerror(res);
        return false;
    }
    // A successful transport-level transfer can still carry an HTTP error
    // (e.g. 4xx/5xx with an empty or error body). Treat those as failures too,
    // otherwise callers happily consume the error body as if it were the
    // requested payload.
    if (http_code >= 400) {
        LOG(ERROR) << "Request failed with HTTP status " << http_code;
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
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file.native_handle());

    // Execute it
    bool result = CURL_perform_common(curl);
    LOG_IF(INFO, result) << "Download succeeded, wrote to " << where;
    return result;
}

std::optional<std::string> download_memory(
    const std::string_view url, CurlUtils::CancelChecker cancel_checker,
    const std::vector<std::string>& headers) {
    std::string result;

    LOG(INFO) << "Downloading " << url << " to memory";
    if (!headers.empty()) {
        LOG(INFO) << "Using " << headers.size()
                  << " custom header(s) for download";
    }

    // Common CURL setup
    CURL* curl = CURL_setup_common(url, cancel_checker);
    if (curl == nullptr) {
        LOG(ERROR) << "Cannot setup curl";
        return std::nullopt;
    }
    CURL_apply_interactive_timeouts(curl);

    // Set request headers if provided
    struct curl_slist* hdrlist = nullptr;
    for (const auto& header : headers) {
        hdrlist = curl_slist_append(hdrlist, header.c_str());
    }
    if (hdrlist != nullptr) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrlist);
    }

    // Write callback
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION,
        +[](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
            auto* out = static_cast<std::string*>(userp);
            if (size != 0 && nmemb > SIZE_MAX / size) {
                return 0;
            }
            const size_t incoming = size * nmemb;
            if (incoming > CurlUtils::kMaxInMemoryResponseBytes - out->size()) {
                LOG(ERROR) << "Response exceeded in-memory limit of "
                           << CurlUtils::kMaxInMemoryResponseBytes
                           << " bytes, aborting";
                return 0;  // short count aborts the transfer
            }
            out->append(static_cast<char*>(contents), incoming);
            LOG_EVERY_N_SEC(INFO, 5)
                << fmt::format("Downloaded {} bytes so far", out->size());
            return incoming;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

    // Execute it
    bool exec_result = CURL_perform_common(curl);
    if (hdrlist != nullptr) {
        curl_slist_free_all(hdrlist);
    }
    LOG_IF(INFO, exec_result) << "Download succeeded";
    if (exec_result) {
        return result;
    } else {
        return std::nullopt;
    }
}

std::optional<std::string> download_memory(
    const std::string_view url, CurlUtils::CancelChecker cancel_checker,
    const std::string_view authkey) {
    std::vector<std::string> headers;
    if (!authkey.empty()) {
        headers.emplace_back("Authorization: Bearer " + std::string(authkey));
    }
    return download_memory(url, std::move(cancel_checker), headers);
}

std::optional<std::string> send_json_get_reply(
    const std::string_view url, std::string json,
    const std::vector<std::string>& headers,
    CurlUtils::CancelChecker cancel_checker) {
    std::string result;

    LOG(INFO) << "Sending JSON to " << url;
    if (json.size() > kMaxJsonRequestBytes) {
        LOG(ERROR) << "JSON request exceeded in-memory limit of "
                   << kMaxJsonRequestBytes << " bytes";
        return {};
    }

    // Common CURL setup
    CURL* curl = CURL_setup_common(url, cancel_checker);
    if (curl == nullptr) {
        LOG(ERROR) << "Cannot setup curl";
        return {};
    }

    CURL_apply_interactive_timeouts(curl);

    struct curl_slist* hdrlist = nullptr;
    hdrlist = curl_slist_append(hdrlist, "Content-Type: application/json");
    for (const auto& header : headers) {
        hdrlist = curl_slist_append(hdrlist, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrlist);

    // Set POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json.size());

    // Write callback
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION,
        +[](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
            auto* out = static_cast<std::string*>(userp);
            if (size != 0 && nmemb > SIZE_MAX / size) {
                return 0;
            }
            const size_t incoming = size * nmemb;
            if (incoming > CurlUtils::kMaxInMemoryResponseBytes - out->size()) {
                LOG(ERROR) << "Reply exceeded in-memory limit of "
                           << CurlUtils::kMaxInMemoryResponseBytes
                           << " bytes, aborting";
                return 0;  // short count aborts the transfer
            }
            out->append(static_cast<char*>(contents), incoming);
            LOG_EVERY_N_SEC(INFO, 5)
                << fmt::format("Received {} bytes so far", out->size());
            return incoming;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

    // Execute it
    bool exec_result = CURL_perform_common(curl);
    curl_slist_free_all(hdrlist);
    LOG_IF(INFO, exec_result) << "Request succeeded";
    if (!exec_result) {
        return {};
    }
    if (result.empty()) {
        LOG(ERROR) << "Received empty reply body from " << url;
        return {};
    }
    return result;
}

std::optional<std::string> send_json_get_reply(const std::string_view url,
                                               std::string json,
                                               const std::string_view authkey,
                                               CancelChecker cancel_checker) {
    std::vector<std::string> headers;
    if (!authkey.empty()) {
        headers.emplace_back("Authorization: Bearer " + std::string(authkey));
    }
    return send_json_get_reply(url, std::move(json), headers,
                               std::move(cancel_checker));
}

}  // namespace CurlUtils
