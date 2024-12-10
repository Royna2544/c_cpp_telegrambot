#include "ToolchainProvider.hpp"

#include <absl/log/log.h>
#include <absl/meta/type_traits.h>
#include <absl/strings/ascii.h>
#include <curl/curl.h>

#include <archives/Tar.hpp>
#include <filesystem>
#include <optional>
#include <system_error>
#include <utils/libfs.hpp>

#include "BytesConversion.hpp"
#include "RepoUtils.hpp"
#include "StructF.hpp"
#include "SystemInfo.hpp"
#include "libsighandler.hpp"

namespace toolchains {

constexpr ConstRepoInfo kGCCARMRepoInfo = {
    "https://github.com/Roynas-Android-Playground/"
    "android_prebuilts_gcc_linux-x86_arm_arm-linux-androideabi-4.9",
    "lineage-19.1",
};

bool GCCAndroidARMProvider::downloadTo(const std::filesystem::path& path) {
    // Download and extract GCC toolchain for Android ARM
    return RepoInfo{kGCCARMRepoInfo}.git_clone(path);
}

constexpr ConstRepoInfo kGCCARM64RepoInfo = {
    "https://github.com/Roynas-Android-Playground/"
    "android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-android-4.9",
    "lineage-19.1",
};

bool GCCAndroidARM64Provider::downloadTo(const std::filesystem::path& path) {
    // Download and extract GCC toolchain for Android ARM64
    return RepoInfo{kGCCARM64RepoInfo}.git_clone(path);
}

std::optional<std::string> ClangProvider::getToolchainTarballURL() const {
    // Download URL from site
    constexpr std::string_view kToolchainLink =
        "https://raw.githubusercontent.com/XSans0/WeebX-Clang/main/main/"
        "link.txt";

    CURL* curl = nullptr;
    CURLcode res{};
    std::string result;
    curl = curl_easy_init();
    if (curl != nullptr) {
        curl_easy_setopt(curl, CURLOPT_URL, kToolchainLink.data());
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
            LOG(ERROR) << "Cannot download toolchain link: aborting...";
            return std::nullopt;
        }
    } else {
        LOG(ERROR) << "Cannot initialize curl";
        return std::nullopt;
    }
    return result;
}

bool ClangProvider::downloadTarball(const std::string_view url,
                                    const std::filesystem::path& where) const {
    std::string result;
    CURL* curl = nullptr;
    CURLcode res{};

    LOG(INFO) << "Downloading Clang toolchain from: " << url;
    curl = curl_easy_init();
    if (curl != nullptr) {
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
                LOG_EVERY_N_SEC(INFO, 5)
                    << fmt::format("Download: {}MB/{}MB", dlnowByte.value(), dltotalByte.value());
                return -static_cast<int>(
                    SignalHandler::isSignaled());  // Continue downloading
            });
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, nullptr);

        // Execute it.
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) {
            LOG(ERROR) << curl_easy_strerror(res);
            LOG(ERROR) << "Cannot download toolchain: aborting...";
            return false;
        }
        LOG(INFO) << "Wrote downloaded tarball to " << where;
        return true;
    } else {
        LOG(ERROR) << "Cannot initialize curl";
        return false;
    }
}

bool ClangProvider::downloadTo(const std::filesystem::path& path) {
    std::filesystem::path kToolchainTarballPath = path / "clang.tar.gz";
    std::error_code ec;

    if (!std::filesystem::exists(kToolchainTarballPath, ec)) {
        if (ec) {
            LOG(ERROR) << "Cannot check existence of toolchain tarball";
            return false;
        }
        if (auto _res = getToolchainTarballURL(); _res) {
            std::string url = *_res;
            absl::StripTrailingAsciiWhitespace(&url);
            if (!downloadTarball(url, kToolchainTarballPath)) {
                return false;
            }
        } else {
            return false;
        }
    } else {
        LOG(INFO) << "Using existing tarball";
    }

    // Extract tarball
    Tar tar(kToolchainTarballPath);
    return tar.extract(path);
}

}  // namespace toolchains