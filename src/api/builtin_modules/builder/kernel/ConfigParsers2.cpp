#include "ConfigParsers2.hpp"

#include <absl/log/log.h>
#include <absl/strings/match.h>
#include <absl/strings/str_replace.h>

#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

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

enum class Optionality {
    OPTIONAL,
    REQUIRED,
};

template <typename T, Optionality opt = Optionality::REQUIRED>
    requires(!is_vector_v<T>)
bool get(const nlohmann::json& value, const std::string_view name, T* result) {
    if (value.contains(name.data())) {
        *result = value[name.data()].get<T>();
        DLOG(INFO) << name << "=" << *result;
    } else if constexpr (opt == Optionality::OPTIONAL) {
        return true;  // Field not present, use default value
    } else {
        LOG(ERROR) << "Missing required field: " << name;
        return false;
    }
    return true;
}

// Overload: Vector types fixup
template <typename T, Optionality opt = Optionality::REQUIRED>
    requires is_vector_v<T>
bool get(const nlohmann::json& value, const std::string_view name, T* result) {
    if (value.contains(name.data())) {
        for (const auto& entry : value[name.data()]) {
            using Elem = fixup_to<typename is_vector<T>::type>::type;
            result->emplace_back(entry.get<Elem>());
            DLOG(INFO) << name << "+=" << entry.get<Elem>();
        }
    } else if constexpr (opt == Optionality::OPTIONAL) {
        return true;  // Field not present, use default value
    } else {
        LOG(ERROR) << "Missing field: " << name;
        return false;
    }
    return true;
}

template <typename T>
    requires(!fixup_to<T>::fix_required)
bool get(const nlohmann::json& value, const std::string_view treename,
         const std::string_view name, T* result) {
    if (value.contains(treename.data())) {
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
bool get(const nlohmann::json& value, const std::string_view treename,
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

bool KernelConfig::parseName(const nlohmann::json& node) {
    StackingError errors;
    errors = get(node, "name", &name);
    underscored_name = absl::StrReplaceAll(name, {
                                                     {" ", "_"},
                                                     {"-", "_"},
                                                     {".", "_"},
                                                 });
    DLOG(INFO) << "underscored_name=" << underscored_name;
    return static_cast<bool>(errors);
}

bool KernelConfig::parseRepoInfo(const nlohmann::json& node) {
    StackingError errors;
    std::string url, branch;
    errors = get(node, "repo", "url", &url);
    errors = get(node, "repo", "branch", &branch);
    // Optional value
    get(node, "repo", "shallow", &shallow_clone);
    repo_info = RepoInfo{std::move(url), std::move(branch)};
    return static_cast<bool>(errors);
}

bool KernelConfig::parseArch(const nlohmann::json& node) {
    StackingError errors;
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
    return static_cast<bool>(errors);
}

bool KernelConfig::parseType(const nlohmann::json& node) {
    StackingError errors;
    std::string typeStr;
    errors = get(node, "type", &typeStr);
    if (typeStr == "Image") {
        type = Type::Image;
    } else if (typeStr == "Image.gz") {
        type = Type::Image_gz;
    } else if (typeStr == "Image.gz-dtb") {
        type = Type::Image_gz_dtb;
    } else if (typeStr == "zImage") {
        type = Type::zImage;
    } else if (typeStr == "zImage-dtb") {
        type = Type::zImage_dtb;
    } else {
        LOG(ERROR) << "Invalid kernel type: " + typeStr;
        return false;
    }
    return static_cast<bool>(errors);
}

bool KernelConfig::parseClangSupport(const nlohmann::json& node) {
    StackingError errors;
    bool supported = false;
    errors = get(node, "toolchains", "clang", &supported);

    if (!supported) {
        clang = ClangSupport::None;
    } else {
        errors = get(node, "toolchains", "llvm_binutils", &supported);
        if (supported) {
            errors = get(node, "toolchains", "llvm_ias", &supported);
            if (supported) {
                clang = ClangSupport::FullLLVMWithIAS;
            } else {
                clang = ClangSupport::FullLLVM;
            }
        } else {
            clang = ClangSupport::Clang;
        }
    }
    return static_cast<bool>(errors);
}

bool KernelConfig::parseAnyKernel(const nlohmann::json& node) {
    if (!node.contains("anykernel")) {
        DLOG(INFO) << "No anykernel info provided";
        return true;
    }
    StackingError errors;
    errors = get(node, "anykernel", "enabled", &anyKernel.enabled);
    if (anyKernel.enabled) {
        errors =
            get(node, "anykernel", "location", &anyKernel.relative_directory);
    }
    return static_cast<bool>(errors);
}
bool KernelConfig::parseDefconfig(const nlohmann::json& node) {
    StackingError errors;
    errors = get(node, "defconfig", "scheme", &defconfig.scheme);
    errors = get(node, "defconfig", "devices", &defconfig.devices);
    return static_cast<bool>(errors);
}
bool KernelConfig::parseFragments(const nlohmann::json& node) {
    if (!node.contains("fragments")) {
        DLOG(INFO) << "No fragments provided";
        return true;
    }
    for (const auto& fragment : node["fragments"]) {
        Fragments frag;
        StackingError int_errors;
        int_errors = get(fragment, "name", &frag.name);
        int_errors = get(fragment, "scheme", &frag.scheme);
        int_errors = get(fragment, "depends", &frag.depends);
        int_errors = get(fragment, "default_enabled", &frag.default_enabled);
        int_errors = get(fragment, "description", &frag.description);
        if (!static_cast<bool>(int_errors)) {
            LOG(ERROR) << "Failed to parse fragment";
            return false;
        }
        DLOG(INFO) << "Parsed fragment: " << frag.name;
        fragments[frag.name] = frag;
    }
    return true;
}
bool KernelConfig::parseEnvMap(const nlohmann::json& node) {
    if (!node.contains("env")) {
        DLOG(INFO) << "No environment variables provided";
        return true;
    }
    for (const auto& env : node["env"]) {
        std::string key, value;
        StackingError int_errors;
        int_errors = get(env, "name", &key);
        int_errors = get(env, "value", &value);
        if (!static_cast<bool>(int_errors)) {
            LOG(ERROR) << "Failed to parse environment variable";
            return false;
        }
        DLOG(INFO) << "ENV: " << key << "=" << value;
        envMap[key] = value;
    }
    return true;
}

bool KernelConfig::parse(const nlohmann::json& node) {
    StackingError errors;
    errors = parseName(node);
    errors = parseRepoInfo(node);
    errors = parseArch(node);
    errors = parseType(node);
    errors = parseClangSupport(node);
    errors = parseAnyKernel(node);
    errors = parseDefconfig(node);
    errors = parseFragments(node);
    errors = parseEnvMap(node);

    if (!static_cast<bool>(errors)) {
        return false;
    }

    DependencyChecker checker(&fragments);
    if (checker.hasDependencyLoop()) {
        LOG(ERROR) << "Dependency loop detected in fragments";
        return false;
    }
    DLOG(INFO) << "Parse complete: " << name;
    return true;
}

void KernelConfig::parse() {
    nlohmann::json root;
    std::ifstream ifs(_sourceFilePath);
    std::string filePath = _sourceFilePath.filename().string();
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open JSON file: " + filePath);
    }
    try {
        fileContentCache = std::string(std::istreambuf_iterator<char>(ifs),
                                       std::istreambuf_iterator<char>());
        root = nlohmann::json::parse(fileContentCache);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("Failed to parse JSON file: " + filePath +
                                 ": " + e.what());
    }
    if (!parse(root)) {
        throw std::runtime_error("Failed to parse JSON file: " + filePath);
    }
}

std::string KernelConfig::toJsonString() const { return fileContentCache; }

bool KernelConfig::reParse() {
    if (!_file.updated()) {
        return false;
    }
    LOG(INFO) << "File has been updated: " << _sourceFilePath.filename();
    parse();
    return true;
}

KernelConfig::KernelConfig(std::filesystem::path jsonFile)
    : _sourceFilePath(std::move(jsonFile)), _file(_sourceFilePath) {
    parse();
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
