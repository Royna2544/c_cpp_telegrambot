#pragma once

#include <fmt/core.h>
#include <json/value.h>

#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../RepoUtils.hpp"
#include "FileWithTimestamp.hpp"

struct KernelConfig {
    std::string name;
    std::string underscored_name;
    RepoInfo repo_info;
    bool shallow_clone = false;
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
    std::unordered_map<std::string, Fragments> fragments;
    std::unordered_map<std::string, std::string> envMap;

    class Patcher {
       protected:
        std::string data1;
        std::string data2;

       public:
        virtual ~Patcher() = default;
        virtual bool apply() = 0;
        Patcher(std::string data1, std::string data2)
            : data1(std::move(data1)), data2(std::move(data2)) {}
    };
    std::vector<std::unique_ptr<Patcher>> patches;

    explicit KernelConfig(std::filesystem::path jsonFile);
    void reParse();

   private:
    bool parseName(const Json::Value& node);
    bool parseRepoInfo(const Json::Value& node);
    bool parseArch(const Json::Value& node);
    bool parseType(const Json::Value& node);
    bool parseClangSupport(const Json::Value& node);
    bool parseAnyKernel(const Json::Value& node);
    bool parseDefconfig(const Json::Value& node);
    bool parseFragments(const Json::Value& node);
    bool parseEnvMap(const Json::Value& node);
    bool parsePatches(const Json::Value& node);
    bool parse(const Json::Value& node);
    void parse();
    std::filesystem::path _sourceFilePath;
    FileWithTimestamp _file;
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
        const std::unordered_map<std::string, Fragments>* fragments)
        : fragments_(fragments) {}

    bool hasDependencyLoop();

   private:
    const std::unordered_map<std::string, Fragments>* fragments_;

    bool detectCycle(const std::string& current,
                     std::unordered_set<std::string>& visited,
                     std::unordered_set<std::string>& recursionStack);
};
