#include "ToolchainProvider.hpp"

#include <absl/log/log.h>
#include <absl/meta/type_traits.h>
#include <absl/strings/ascii.h>

#include <archives/Tar.hpp>
#include <filesystem>
#include <optional>
#include <system_error>
#include <utils/libfs.hpp>

#include "CurlUtils.hpp"
#include "RepoUtils.hpp"
#include "SystemInfo.hpp"

namespace toolchains {

constexpr ConstRepoInfo kGCCARMRepoInfo = {
    "https://github.com/Roynas-Android-Playground/"
    "android_prebuilts_gcc_linux-x86_arm_arm-linux-androideabi-4.9",
    "lineage-19.1",
};

bool GCCAndroidARMProvider::downloadTo(const std::filesystem::path& path) {
    // Download and extract GCC toolchain for Android ARM
    return RepoInfo{kGCCARMRepoInfo}.git_clone(path, std::nullopt, true);
}

constexpr ConstRepoInfo kGCCARM64RepoInfo = {
    "https://github.com/Roynas-Android-Playground/"
    "android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-android-4.9",
    "lineage-19.1",
};

bool GCCAndroidARM64Provider::downloadTo(const std::filesystem::path& path) {
    // Download and extract GCC toolchain for Android ARM64
    return RepoInfo{kGCCARM64RepoInfo}.git_clone(path, std::nullopt, true);
}

bool ClangProvider::downloadTarball(const std::string_view url,
                                    const std::filesystem::path& where) const {
    return CURL_download_file(url, where);
}

bool ClangProvider::downloadTo(const std::filesystem::path& path) {
    std::filesystem::path kToolchainTarballPath = path / "clang.tar.gz";
    std::error_code ec;

    if (!std::filesystem::exists(kToolchainTarballPath, ec)) {
        if (ec) {
            LOG(ERROR) << "Cannot check existence of toolchain tarball";
            return false;
        }
        std::string url =
            "https://android.googlesource.com/platform/prebuilts/clang/host/"
            "linux-x86/+archive/refs/tags/android-16.0.0_r4/"
            "clang-r547379.tar.gz";
        if (!downloadTarball(url, kToolchainTarballPath)) {
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
