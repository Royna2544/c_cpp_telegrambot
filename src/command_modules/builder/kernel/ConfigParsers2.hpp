#pragma once

#include <fmt/core.h>
#include <json/value.h>

#include <filesystem>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "../RepoUtils.hpp"

struct KernelConfig {
    std::string name;
    std::string underscored_name;
    RepoInfo repo_info;
    enum class Arch { ARM = 1, ARM64, X86, X86_64, MAX = X86_64 } arch{};
    enum class Type {
        Image,
        Image_gz,
        Image_gz_dtb,
    } type{};
    enum class ClangSupport {
        None,            // GCC only supported
        Clang,           // GNU binutils with CC=clang
        FullLLVM,        // Fully compilable with LLVM
        FullLLVMWithIAS  // Additional support for LLVM=1 LLVM_IAS=1
    } clang{};
    struct AnyKernel {
        bool enabled;
        std::filesystem::path relative_directory;
    } anyKernel{};
    struct Defconfig {
        std::string scheme;
        std::vector<std::string> devices;
    } defconfig;
    struct Fragments {
        std::string name;
        std::string scheme;
        std::vector<std::string> depends;
        std::string description;
        bool default_enabled;
    };
    std::map<std::string, Fragments> fragments;
    std::map<std::string, std::string> envMap;

    bool parse(const Json::Value& node);
    explicit KernelConfig(const std::filesystem::path& jsonFile);
};

template <>
struct fmt::formatter<KernelConfig::Arch> : formatter<string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(KernelConfig::Arch c,
                format_context& ctx) const -> format_context::iterator {
        string_view name = "unknown";
        switch (c) {
            case KernelConfig::Arch::ARM:
                name = "arm";
                break;
            case KernelConfig::Arch::ARM64:
                name = "arm64";
                break;
            case KernelConfig::Arch::X86:
                name = "x86";
                break;
            case KernelConfig::Arch::X86_64:
                name = "x86_64";
                break;
            default:
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<KernelConfig::Type> : formatter<string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(KernelConfig::Type c,
                format_context& ctx) const -> format_context::iterator {
        string_view name = "unknown";
        switch (c) {
            case KernelConfig::Type::Image:
                name = "Image";
                break;
            case KernelConfig::Type::Image_gz:
                name = "Image.gz";
                break;
            case KernelConfig::Type::Image_gz_dtb:
                name = "Image.gz-dtb";
                break;
            default:
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

inline std::filesystem::path operator/(std::filesystem::path lhs,
                                       KernelConfig::Arch rhs) {
    // This is how the Linux kernel source tree is
    if (rhs == KernelConfig::Arch::X86_64) {
        rhs = KernelConfig::Arch::X86;
    }
    return lhs /= fmt::format("{}", rhs);
}

inline std::filesystem::path operator/(std::filesystem::path lhs,
                                       const KernelConfig::Type rhs) {
    return lhs /= fmt::format("{}", rhs);
}

class DependencyChecker {
   public:
    using Fragments = KernelConfig::Fragments;

    explicit DependencyChecker(
        const std::map<std::string, Fragments>* fragments)
        : fragments_(fragments) {}

    bool hasDependencyLoop();

   private:
    const std::map<std::string, Fragments>* fragments_;

    bool detectCycle(const std::string& current,
                     std::unordered_set<std::string>& visited,
                     std::unordered_set<std::string>& recursionStack);
};
