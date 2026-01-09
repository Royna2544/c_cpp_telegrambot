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

constexpr std::string_view GCC_ARM_RepoLink =
    "https://github.com/Roynas-Android-Playground/"
    "android_prebuilts_gcc_linux-x86_arm_arm-linux-androideabi-4.9";
constexpr std::string_view GCC_ARM64_RepoLink =
    "https://github.com/Roynas-Android-Playground/"
    "android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-android-4.9";
constexpr std::string_view GCC_Repobranch = "lineage-19.1";

constexpr std::string_view Clang_RepoLink =
    "https://android.googlesource.com/platform/prebuilts/clang/host/"
    "linux-x86/+archive/refs/tags/android-16.0.0_r4/"
    "clang-r547379.tar.gz";

constexpr ConstRepoInfo kGCCARMRepoInfo = {
    .url = GCC_ARM_RepoLink,
    .branch = GCC_Repobranch,
};
constexpr ConstRepoInfo kGCCARM64RepoInfo = {
    .url = GCC_ARM64_RepoLink,
    .branch = GCC_Repobranch,
};

bool GCCAndroidARMProvider::downloadTo(const std::filesystem::path& path) {
    // Download and extract GCC toolchain for Android ARM
    return RepoInfo{kGCCARMRepoInfo}.git_clone(path, std::nullopt, true);
}

bool GCCAndroidARM64Provider::downloadTo(const std::filesystem::path& path) {
    // Download and extract GCC toolchain for Android ARM64
    return RepoInfo{kGCCARM64RepoInfo}.git_clone(path, std::nullopt, true);
}

bool ClangProvider::downloadTo(const std::filesystem::path& path) {
    std::filesystem::path kToolchainTarballPath = path / "clang.tar.gz";
    std::error_code ec;

    if (!std::filesystem::exists(kToolchainTarballPath, ec)) {
        if (ec) {
            LOG(ERROR) << "Cannot check existence of toolchain tarball";
            return false;
        }
        if (!CurlUtils::download_file(Clang_RepoLink, kToolchainTarballPath)) {
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
