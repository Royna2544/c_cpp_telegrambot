#include "ToolchainProvider.hpp"

#include <absl/log/log.h>
#include <curl/curl.h>

#include <archives/Tar.hpp>
#include <filesystem>
#include <system_error>
#include <utils/libfs.hpp>

#include "RepoUtils.hpp"
#include "StructF.hpp"

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

bool ClangProvider::downloadTo(const std::filesystem::path& path) {
    // Download URL from site
    constexpr std::string_view kToolchainLink =
        "https://raw.githubusercontent.com/XSans0/WeebX-Clang/main/main/"
        "link.txt";

    std::filesystem::path kToolchainTarballPath = path / "clang.tar.gz";
    std::error_code ec;

    if (!std::filesystem::exists(kToolchainTarballPath, ec)) {
        if (ec) {
            LOG(ERROR)
                << "Cannot check existence of toolchain tarball: aborting...";
            return false;
        }
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
                return false;
            }
        } else {
            LOG(ERROR) << "Cannot initialize curl";
            return false;
        }
        std::istringstream iss(result);
        std::string clang_url;
        iss >> clang_url;
        LOG(INFO) << "Downloading Clang toolchain from: " << clang_url;
        result.clear();
        curl = curl_easy_init();
        if (curl != nullptr) {
            curl_easy_setopt(curl, CURLOPT_URL, clang_url.c_str());
            // Follow up to 5 redirects
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
            // Enable 302 redirects
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            if (createDirectory(path)) {
                LOG(ERROR) << "Cannot create directory: " << path;
                return false;
            }
            F file;
            if (!file.open(kToolchainTarballPath, F::Mode::WriteBinary)) {
                LOG(ERROR) << "Cannot open file for writing";
                return false;
            }
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<FILE*>(file));
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            if (res != CURLE_OK) {
                LOG(ERROR) << curl_easy_strerror(res);
                LOG(ERROR) << "Cannot download toolchain: aborting...";
                return false;
            }
            LOG(INFO) << "Wrote downloaded tarball to "
                      << kToolchainTarballPath;
        } else {
            LOG(ERROR) << "Cannot initialize curl";
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