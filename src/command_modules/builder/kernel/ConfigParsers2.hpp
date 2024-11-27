#pragma once

#include <json/value.h>

#include <filesystem>
#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <unordered_set>
#include <vector>

#include "../RepoUtils.hpp"

struct KernelConfig {
    std::string name;
    std::string underscored_name;
    using RepoInfo = RepoInfo;
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
        std::vector<std::string> target_devices;
        std::string description;
        bool default_enabled;
    };
    std::map<std::string, Fragments> fragments;

    bool parse(const Json::Value& node);
    explicit KernelConfig(const std::filesystem::path& jsonFile);
};

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
