#include "ConfigParsers2.hpp"

#include <absl/log/log.h>
#include <absl/strings/match.h>

#include <filesystem>
#include <fstream>
#include <type_traits>
#include <vector>

#include "json/reader.h"

// vector
template <typename T>
struct is_vector : std::false_type {};
template <typename T>
struct is_vector<std::vector<T>> : std::true_type {
    using type = T;
};
template <typename T>
constexpr bool is_vector_v = is_vector<T>::value;

template <typename T>
struct fixup_to {
    using type = T;
    constexpr static bool fix_required = false;
};

template <>
struct fixup_to<std::filesystem::path> {
    using type = std::string;
    constexpr static bool fix_required = true;
};

template <typename T>
    requires(!is_vector_v<T>)
bool get(const Json::Value& value, const std::string_view name, T* result) {
    if (value.isMember(name.data())) {
        *result = value[name.data()].as<T>();
        DLOG(INFO) << name << "=" << *result;
    } else {
        LOG(ERROR) << "Missing required field: " << name;
        return false;
    }
    return true;
}

// Overload: Vector types fixup
template <typename T>
    requires is_vector_v<T>
bool get(const Json::Value& value, const std::string_view name, T* result) {
    if (value.isMember(name.data())) {
        for (const auto& entry : value[name.data()]) {
            using Elem = fixup_to<typename is_vector<T>::type>::type;
            result->emplace_back(entry.as<Elem>());
            DLOG(INFO) << name << "+=" << entry.as<Elem>();
        }
    } else {
        LOG(ERROR) << "Missing required field: " << name;
        return false;
    }
    return true;
}

template <typename T>
    requires(!fixup_to<T>::fix_required)
bool get(const Json::Value& value, const std::string_view treename,
         const std::string_view name, T* result) {
    if (value.isMember(treename.data())) {
        return get<T>(value[treename.data()], name, result);
    } else {
        LOG(ERROR) << "Missing required node: " << name;
        return false;
    }
    return true;
}

// Overload: With fixup
template <typename T>
    requires fixup_to<T>::fix_required
bool get(const Json::Value& value, const std::string_view treename,
         const std::string_view name, T* result) {
    typename fixup_to<T>::type _result;
    if (get<typename fixup_to<T>::type>(value, treename, name, &_result)) {
        *result = _result;
        return true;
    }
    return false;
}

struct StackingError {
    int failures{};

    StackingError& operator=(bool rhs) {
        if (!rhs) {
            failures++;
        }
        return *this;
    }
    explicit operator bool() const { return failures == 0; }
    ~StackingError() {
        if (failures > 0) {
            LOG(ERROR) << "Total parse errors: " << failures;
        }
    }
};

bool KernelConfig::parse(const Json::Value& node) {
    StackingError errors;
    errors = get(node, "name", &name);
    errors = get(node, "repo", "url", &repo_info.url);
    errors = get(node, "repo", "branch", &repo_info.branch);
    std::string archStr;
    errors = get(node, "arch", &archStr);
    if (absl::EqualsIgnoreCase(archStr, "arm")) {
        arch = Arch::ARM;
    } else if (absl::EqualsIgnoreCase(archStr, "arm64")) {
        arch = Arch::ARM64;
    } else if (absl::EqualsIgnoreCase(archStr, "x86")) {
        arch = Arch::X86;
    } else if (absl::EqualsIgnoreCase(archStr, "x86_64")) {
        arch = Arch::X86_64;
    } else {
        LOG(ERROR) << "Invalid kernel architecture: " << archStr;
        return false;
    }
    std::string typeStr;
    errors = get(node, "type", &typeStr);
    if (typeStr == "Image") {
        type = Type::Image;
    } else if (typeStr == "Image.gz") {
        type = Type::Image_gz;
    } else if (typeStr == "Image.gz-dtb") {
        type = Type::Image_gz_dtb;
    } else {
        LOG(ERROR) << "Invalid kernel type: " + typeStr;
        return false;
    }
    bool clangsupported = false;
    bool clangbinutilssupported = false;
    bool clangiasupported = false;
    errors = get(node, "toolchains", "Clang", &clangsupported);
    errors = get(node, "toolchains", "LLVM Binutils", &clangbinutilssupported);
    errors = get(node, "toolchains", "LLVM IAS", &clangiasupported);

    if (!clangsupported) {
        clang = ClangSupport::None;
    } else {
        if (clangbinutilssupported) {
            if (clangiasupported) {
                clang = ClangSupport::FullLLVMWithIAS;
            } else {
                clang = ClangSupport::FullLLVM;
            }
        } else {
            clang = ClangSupport::Clang;
        }
    }
    errors = get(node, "anykernel", "enabled", &anyKernel.enabled);
    if (anyKernel.enabled) {
        errors =
            get(node, "anykernel", "location", &anyKernel.relative_directory);
        errors = get(node, "anykernel", "additionalFiles",
                     &anyKernel.additional_files);
    }
    errors = get(node, "defconfig", "scheme", &defconfig.scheme);
    errors = get(node, "defconfig", "devices", &defconfig.devices);
    for (const auto& fragment : node["fragments"]) {
        Fragments frag;
        StackingError int_errors;
        int_errors = get(fragment, "name", &frag.name);
        int_errors = get(fragment, "scheme", &frag.scheme);
        int_errors = get(fragment, "depends", &frag.depends);
        int_errors = get(fragment, "devices", &frag.target_devices);
        int_errors = get(fragment, "default_enabled", &frag.default_enabled);
        int_errors = get(fragment, "description", &frag.description);
        DLOG(INFO) << "Parsed fragment: " << frag.name;
        fragments[frag.name] = frag;
    }
    DependencyChecker checker(&fragments);
    if (checker.hasDependencyLoop()) {
        LOG(ERROR) << "Dependency loop detected in fragments";
        return false;
    }
    DLOG(INFO) << "Parse complete: " << name;
    return true;
}

KernelConfig::KernelConfig(const std::filesystem::path& jsonFile) {
    Json::Value root;
    std::ifstream ifs(jsonFile);
    std::string filePath = jsonFile.filename().string();
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open JSON file: " + filePath);
    }
    Json::Reader reader;
    if (!reader.parse(ifs, root, false)) {
        throw std::runtime_error("Failed to parse JSON file: " + filePath);
    }
    if (!parse(root)) {
        throw std::runtime_error("Failed to parse JSON file: " + filePath);
    }
}

bool DependencyChecker::hasDependencyLoop() {
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursionStack;

    for (const auto& [name, fragment] : *fragments_) {
        if (visited.find(name) == visited.end()) {
            if (detectCycle(name, visited, recursionStack)) {
                return true;  // Cycle detected
            }
        }
    }
    return false;
}

bool DependencyChecker::detectCycle(
    const std::string& current, std::unordered_set<std::string>& visited,
    std::unordered_set<std::string>& recursionStack) {
    visited.insert(current);
    recursionStack.insert(current);

    auto it = fragments_->find(current);
    if (it != fragments_->end()) {
        for (const auto& dep : it->second.depends) {
            if (recursionStack.find(dep) != recursionStack.end()) {
                LOG(ERROR) << "Dependency loop detected: " << dep << " <- "
                           << current;
                return true;  // Cycle detected
            }
            if (visited.find(dep) == visited.end()) {
                if (detectCycle(dep, visited, recursionStack)) {
                    return true;  // Cycle detected in recursion
                }
            }
        }
    }

    recursionStack.erase(current);
    return false;
}