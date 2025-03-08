#include "ConfigParsers.hpp"

#include <absl/strings/strip.h>
#include <fmt/format.h>
#include <json/json.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BytesConversion.hpp"
#include "SystemInfo.hpp"
#include "json/value.h"

enum class ValueType {
    nullValue = Json::nullValue,
    intValue = Json::intValue,
    uintValue = Json::uintValue,
    realValue = Json::realValue,
    stringValue = Json::stringValue,
    arrayValue = Json::arrayValue,
    objectValue = Json::objectValue
};

template <>
struct fmt::formatter<ValueType> : formatter<std::string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(ValueType c, format_context& ctx) const
        -> format_context::iterator {
        string_view name = "unknown";
        switch (c) {
            case ValueType::nullValue:
                name = "nullValue";
                break;
            case ValueType::intValue:
                name = "intValue";
                break;
            case ValueType::uintValue:
                name = "uintValue";
                break;
            case ValueType::realValue:
                name = "realValue";
                break;
            case ValueType::stringValue:
                name = "stringValue";
                break;
            case ValueType::arrayValue:
                name = "arrayValue";
                break;
            case ValueType::objectValue:
                name = "objectValue";
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

// Repesent a type and name string
template <typename T>
struct NodeItemType {
    using type = std::decay_t<T>;

   private:
    std::string name;
    bool required = true;
    std::optional<T> value_;
    bool hasDefaultValue = false;

   public:
    NodeItemType(std::string name) : name(std::move(name)) {}
    NodeItemType& setRequired(bool required) {
        this->required = required;
        return *this;
    }
    NodeItemType& setDefaultValue(const T& defaultValue) {
        value_ = defaultValue;
        hasDefaultValue = true;
        required = false;
        return *this;
    }

    NodeItemType& setValue(const T& value) {
        static_assert(std::is_same_v<T, std::decay_t<T>>,
                      "Value type must match node type");
        value_ = value;
        return *this;
    }
    operator T() const {
        if (!hasValue()) {
            throw std::invalid_argument("Value must have value to get");
        }
        return value_.value();
    }
    [[nodiscard]] bool hasValue() const {
        return hasDefaultValue || value_ != std::nullopt;
    }

    template <typename... Args>
    friend bool checkRequirements(const Json::Value& value,
                                  NodeItemType<Args>&... args);
};

NodeItemType<std::string> operator""_s(const char* str,
                                       unsigned long /*unused*/) {
    return {str};
}
NodeItemType<int> operator""_d(const char* str, unsigned long /*unused*/) {
    return {str};
}

template <typename... Args>
bool checkRequirements(const Json::Value& value, NodeItemType<Args>&... args) {
    static_assert(sizeof...(args) > 0,
                  "At least one argument required for checkRequirements");

    if (!value.isObject()) {
        LOG(ERROR) << fmt::format("Expected object, found: {}",
                                  static_cast<ValueType>(value.type()));
        return false;
    }

    bool allRequiredMet = true;
    (([&allRequiredMet, value, &args] {
         if (!value.isMember(args.name)) {
             if (args.hasDefaultValue) {
                 LOG(INFO) << "Using default value for field: " << args.name;
             } else if (args.required) {
                 allRequiredMet = false;
                 LOG(WARNING) << "Missing field: " << args.name;
                 return;
             }
         } else {
             const auto& node = value[args.name];
             if constexpr (std::is_same_v<
                               typename std::decay_t<decltype(args)>::type,
                               int>) {
                 if (!node.isInt()) {
                     LOG(ERROR) << "Expected integer for field: " << args.name;
                     allRequiredMet = false;
                 } else {
                     args.setValue(node.asInt());
                 }
             }
             if constexpr (std::is_same_v<
                               typename std::decay_t<decltype(args)>::type,
                               std::string>) {
                 if (!node.isString()) {
                     LOG(ERROR) << "Expected string for field: " << args.name;
                     allRequiredMet = false;
                 } else {
                     args.setValue(node.asString());
                 }
             }
         }
     })(),
     ...);

    return allRequiredMet;
}

/**
 * @brief Proxy class for handling JSON objects and providing iterator access.
 *
 * This class wraps a Json::Value object and provides iterator access to its
 * members. It also performs basic validation to ensure that the root value is
 * an object.
 *
 * @param root The Json::Value object to be wrapped.
 */
class ProxyJsonBranch {
   public:
    /**
     * @brief Returns an iterator pointing to the beginning of the wrapped JSON
     * object.
     *
     * @return An iterator pointing to the beginning of the wrapped JSON object.
     */
    [[nodiscard]] Json::Value::const_iterator begin() const {
        return isValidObject ? root_.begin() : root_.end();
    }

    /**
     * @brief Returns an iterator pointing to the end of the wrapped JSON
     * object.
     *
     * @return An iterator pointing to the end of the wrapped JSON object.
     */
    [[nodiscard]] Json::Value::const_iterator end() const {
        return root_.end();
    }

    /**
     * @brief Constructor for ProxyJsonBranch.
     *
     * Constructs a ProxyJsonBranch object and performs basic validation on the
     * provided Json::Value object. If the root value is not an array, it logs
     * an error message.
     *
     * @param root The Json::Value object to be wrapped.
     */
    explicit ProxyJsonBranch(Json::Value root, const std::string& name)
        : root_(root[name]), isValidObject(root_.isArray()) {
        if (!isValidObject) {
            LOG(ERROR) << fmt::format(
                "Expected object, found: {} while accessing property '{}'",
                static_cast<ValueType>(root_.type()), name);
            switch (root_.type()) {
                case Json::ValueType::nullValue:
                    LOG(WARNING) << "Note: Root value is not an object, means "
                                    "it is nonexistent in the tree.";
                    break;
                default:
                    // TODO: Add more helper messages for different
                    // Json::ValueType
                    break;
            }
        }
    }

   private:
    Json::Value root_;
    bool isValidObject;
};

namespace ParseFiles {

std::vector<ConfigParser::ROMBranch::Ptr> parseROM(
    Json::Value entry, const std::string& default_target,
    ConfigParser::MatcherStorage* storage) {
    auto romInfo = std::make_shared<ConfigParser::ROMInfo>();
    auto ROMName = "name"_s;
    auto ROMLink = "link"_s;
    auto ROMTarget = "target"_s.setDefaultValue(default_target);
    std::vector<ConfigParser::ROMBranch::Ptr> romBranches;

    if (!checkRequirements(entry, ROMName, ROMLink, ROMTarget)) {
        LOG(INFO) << "Skipping invalid roms entry: " << entry.toStyledString();
        return {};
    }
    if (!entry.isMember("artifact")) {
        LOG(INFO) << fmt::format("Artifact entry not found for '{}'.",
                                 static_cast<std::string>(ROMName));
    } else {
        auto matcher = "matcher"_s;
        auto data = "data"_s;
        if (!checkRequirements(entry["artifact"], matcher, data)) {
            LOG(INFO) << "Skipping invalid artifact entry: "
                      << entry.toStyledString();
            return {};
        }
        romInfo->artifact = storage->get(matcher);
        if (!romInfo->artifact) {
            LOG(INFO) << "No matching ArtifactMatcher for name "
                      << static_cast<std::string>(matcher);
            return {};
        }
        romInfo->artifact->setData(data);
    }

    romInfo->name = ROMName;
    romInfo->url = ROMLink;
    romInfo->target = ROMTarget;
    DLOG(INFO) << "Parsed ROM: " << romInfo->name;

    for (const auto& branchEntry : ProxyJsonBranch(entry, "branches")) {
        auto androidVersion = "android_version"_d;
        auto branch = "branch"_s;
        if (!checkRequirements(branchEntry, androidVersion, branch)) {
            LOG(INFO) << "Skipping invalid roms-branches entry: "
                      << entry.toStyledString();
            continue;
        }
        ConfigParser::ROMBranch rombranch;
        rombranch.branch = branch;
        rombranch.androidVersion = androidVersion;
        rombranch.romInfo = romInfo;
        romBranches.emplace_back(
            std::make_shared<ConfigParser::ROMBranch>(rombranch));
    }
    return romBranches;
}

struct ROM {
    using return_type = ConfigParser::ROMBranch::Ptr;
    static constexpr bool kReturnVec = true;
    static constexpr Json::ValueType _value = Json::arrayValue;
    static std::vector<return_type> parse(
        const Json::Value& root, ConfigParser::MatcherStorage* storage) {
        std::vector<return_type> result;
        for (const auto& entry : root) {
            auto res = parseROM(entry, "bacon", storage);
            if (!res.empty()) {
                result.insert(result.end(), res.begin(), res.end());
            }
        }
        return result;
    }
};

struct Recovery {
    using return_type = ConfigParser::ROMBranch::Ptr;
    static constexpr bool kReturnVec = true;
    static constexpr Json::ValueType _value = Json::arrayValue;
    static std::vector<return_type> parse(
        const Json::Value& root, ConfigParser::MatcherStorage* storage) {
        std::vector<return_type> result;
        for (const auto& entry : root) {
            auto res = parseROM(entry, "recoveryimage", storage);
            if (!res.empty()) {
                result.insert(result.end(), res.begin(), res.end());
            }
        }
        return result;
    }
};

using DeviceMap = std::unordered_map<std::string, ConfigParser::Device::Ptr>;
using ROMsMap = std::unordered_map<std::string, ConfigParser::ROMBranch::Ptr>;
using Devices = std::vector<ConfigParser::Device::Ptr>;
using ROMs = std::vector<ConfigParser::ROMBranch::Ptr>;

std::pair<Devices, ROMs> parseDeviceAndRom(
    const NodeItemType<std::string>& device, const Json::Value& rootNode,
    const DeviceMap& devicesPtr, const ROMsMap& romsPtr,
    const std::string_view target_rom, const int android_version) {
    ROMs romBranch;
    std::vector<std::string> deviceCodenames;

    // Parse codenames array
    if (device.hasValue()) {
        deviceCodenames.emplace_back(device);
    } else {
        for (const auto& deviceEntry : ProxyJsonBranch(rootNode, "devices")) {
            deviceCodenames.emplace_back(deviceEntry.asString());
        }
    }
    if (deviceCodenames.empty()) {
        LOG(INFO) << "No device codenames found";
        return {};
    }

    // Obtain pointers to device, else create dummy
    std::vector<ConfigParser::Device::Ptr> devicePtrs;
    std::ranges::transform(
        deviceCodenames, std::back_inserter(devicePtrs),
        [&devicesPtr](const auto& codename) {
            if (!devicesPtr.contains(codename)) {
                LOG(WARNING)
                    << "No matching DeviceInfo for codename " << codename;
                // Create dummy device if it doesn't exist, But this isn't gonna
                // make it available.
                return std::make_shared<ConfigParser::Device>(codename);
            }
            return devicesPtr.at(codename);
        });

    // Find the matching ROM
    ConfigParser::ROMBranch::Ptr lookup = nullptr;
    auto key = ConfigParser::ROMBranch::makeKey(
        static_cast<std::string>(target_rom), android_version);

    if (romsPtr.contains(key)) {
        romBranch.emplace_back(romsPtr.at(key));
    } else if (target_rom == "*") {
        for (const auto& romEntry : romsPtr) {
            if (romEntry.second->androidVersion == android_version) {
                romBranch.emplace_back(romEntry.second);
            }
        }
    } else {
        LOG(WARNING) << "No matching ROM for target_rom " << target_rom
                     << " and android_version " << android_version;
        return {};
    }
    return std::make_pair(std::move(devicePtrs), std::move(romBranch));
}

struct ROMLocalManifest {
    using return_type = ConfigParser::LocalManifest::Ptr;
    static constexpr bool kReturnVec = true;
    static constexpr Json::ValueType _value = Json::objectValue;
    static std::vector<return_type> parseOneLocalManifest(
        const Json::Value& json, const DeviceMap& devices, const ROMsMap& roms,
        std::string name, std::string manifest) {
        ConfigParser::LocalManifest localManifest;
        std::vector<return_type> result;
        auto branchName = "name"_s;
        auto target_rom = "target_rom"_s;
        auto android_version = "android_version"_d;
        auto device = "device"_s.setRequired(false);

        if (!checkRequirements(json, target_rom, branchName, android_version,
                               device)) {
            LOG(INFO) << "Skipping invalid localmanifest-branches entry: "
                      << json.toStyledString();
            return {};
        }

        auto [devicesVec, rom] = parseDeviceAndRom(
            device, json, devices, roms, static_cast<std::string>(target_rom),
            android_version);

        if (devicesVec.empty() && rom.empty()) {
            LOG(INFO) << "No matching device or ROM for localmanifest-branches "
                      << json.toStyledString();
            return {};
        }
        // Assign to the localmanifest
        localManifest.devices = devicesVec;
        localManifest.name = std::move(name);
        localManifest.preparar =
            std::make_shared<ConfigParser::LocalManifest::GitPrepare>(
                RepoInfo{std::move(manifest), branchName});
        {
            static int job_count = [] {
                const auto totalMem = GigaBytes(MemoryInfo().totalMemory);
                LOG(INFO) << "Total memory: " << totalMem;
                int _job_count = static_cast<int>(totalMem.value() / 4 - 1);
                LOG(INFO) << "Using job count: " << _job_count
                          << " for ROM build";
                return _job_count;
            }();
            localManifest.job_count = job_count;
        }
        for (const auto& romi : rom) {
            localManifest.rom = romi;
            result.emplace_back(
                std::make_shared<ConfigParser::LocalManifest>(localManifest));
        }
        return result;
    }
    static std::vector<return_type> parse(const Json::Value& root,
                                          const DeviceMap& devices,
                                          const ROMsMap& roms) {
        auto name = "name"_s;
        auto manifest = "url"_s;
        if (!checkRequirements(root, name, manifest)) {
            LOG(INFO) << "Skipping invalid localmanifest entry: "
                      << root.toStyledString();
            return {};
        }
        std::vector<return_type> returnVec;
        for (const auto& manifestEntry : ProxyJsonBranch(root, "branches")) {
            auto localManifest = parseOneLocalManifest(manifestEntry, devices,
                                                       roms, name, manifest);
            if (!localManifest.empty()) {
                returnVec.insert(returnVec.end(), localManifest.begin(),
                                 localManifest.end());
            }
        }
        return returnVec;
    }
};

struct RecoveryManifest {
    using return_type = ConfigParser::LocalManifest::Ptr;
    static constexpr Json::ValueType _value = Json::objectValue;
    static std::vector<return_type> parse(const Json::Value& root,
                                          const DeviceMap& devices,
                                          const ROMsMap& roms) {
        std::vector<return_type> manifests;
        ConfigParser::LocalManifest localManifest;
        auto name = "name"_s;
        auto android_version = "android_version"_d;
        auto device = "device"_s.setRequired(false);
        auto recoveryName = "target_recovery"_s;

        if (!checkRequirements(root, name, android_version, device,
                               recoveryName)) {
            LOG(INFO) << "Skipping invalid recovery_targets entry: "
                      << root.toStyledString();
            return {};
        }

        auto [devicesVec, rom] = parseDeviceAndRom(
            device, root, devices, roms, static_cast<std::string>(recoveryName),
            android_version);

        auto prepare =
            std::make_shared<ConfigParser::LocalManifest::WritePrepare>();
        for (const auto& mapping : ProxyJsonBranch(root, "clone_mapping")) {
            auto link = "link"_s;
            auto branch = "branch"_s;
            auto destination = "destination"_s;
            if (!checkRequirements(mapping, link, branch, destination)) {
                LOG(INFO) << "Skipping invalid clone_mapping entry: "
                          << mapping.toStyledString();
                continue;
            }
            prepare->data.emplace_back(link, branch,
                                       std::filesystem::path(destination));
        }
        localManifest.name = name;
        localManifest.preparar = std::move(prepare);
        localManifest.devices = std::move(devicesVec);
        localManifest.job_count = std::thread::hardware_concurrency();
        for (const auto& romi : rom) {
            localManifest.rom = romi;
            manifests.emplace_back(
                std::make_shared<ConfigParser::LocalManifest>(localManifest));
        }
        return manifests;
    }
};

struct DeviceNames {
    using return_type = ConfigParser::Device::Ptr;
    static constexpr bool kReturnVec = false;
    static constexpr Json::ValueType _value = Json::arrayValue;
    static std::vector<return_type> parse(const Json::Value& root) {
        std::vector<return_type> devices;
        for (const auto& entry : root) {
            auto codename = "codename"_s;
            auto name = "name"_s;

            if (!checkRequirements(entry, codename, name)) {
                LOG(INFO) << "Skipping invalid target entry: "
                          << entry.toStyledString();
                return {};
            }
            auto device = std::make_shared<ConfigParser::Device>();
            device->codename = codename;
            device->marketName = name;
            DLOG(INFO) << "Parsed device: " << device->codename;
            devices.emplace_back(std::move(device));
        }
        return devices;
    }
};

template <typename T, typename... Args>
std::vector<typename T::return_type> parse(
    const std::filesystem::path& jsonFile, Args&&... args) {
    Json::Value value;
    Json::Reader reader;

    std::ifstream file(jsonFile);
    if (!file.is_open()) {
        LOG(ERROR) << fmt::format("Failed to open {}",
                                  jsonFile.filename().string());
        return {};
    }

    std::string doc{std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>{}};
    if (!reader.parse(doc, value)) {
        LOG(ERROR) << fmt::format("Failed to parse {}: {}",
                                  jsonFile.filename().string(),
                                  reader.getFormattedErrorMessages());
        return {};
    }
    if (value.type() != T::_value) {
        LOG(ERROR) << fmt::format(
            "Expected {} for {} (actual {})", static_cast<ValueType>(T::_value),
            jsonFile.filename().string(), static_cast<ValueType>(value.type()));
        return {};
    }
    return T::parse(value, std::forward<Args&&>(args)...);
}

bool is_json_file(const std::filesystem::path& path) {
    std::error_code ec;
    return path.extension() == ".json" &&
           std::filesystem::is_regular_file(path, ec);
}
}  // namespace ParseFiles

bool ConfigParser::merge() {
    auto devices = ParseFiles::parse<ParseFiles::DeviceNames>(_jsonFileDir /
                                                              "targets.json");

    // Parse all the components
    auto roms = ParseFiles::parse<ParseFiles::ROM>(_jsonFileDir / "roms.json",
                                                   &storage);
    auto recoveries = ParseFiles::parse<ParseFiles::Recovery>(
        _jsonFileDir / "recoveries.json", &storage);

    for (const auto& device : devices) {
        deviceMap[device->codename] = device;
    }
    for (const auto& rom : roms) {
        romBranchMap[rom->makeKey()] = rom;
    }
    for (const auto& recovery : recoveries) {
        romBranchMap[recovery->makeKey()] = recovery;
    }

    std::error_code ec;

    // Scan through manifest/
    for (const auto& dirit :
         std::filesystem::directory_iterator(_jsonFileDir / "manifest", ec)) {
        if (ParseFiles::is_json_file(dirit.path())) {
            auto manifests = ParseFiles::parse<ParseFiles::ROMLocalManifest>(
                dirit.path(), deviceMap, romBranchMap);
            parsedManifests.insert(parsedManifests.end(), manifests.begin(),
                                   manifests.end());
            DLOG(INFO) << "Add file: " << dirit.path().filename() << ": "
                       << manifests.size();
        }
    }
    if (ec) {
        LOG(ERROR) << "Error scanning manifest directory: " << ec.message();
        return false;
    }

    // Scan through manifest/recovery
    for (const auto& dirit : std::filesystem::directory_iterator(
             _jsonFileDir / "manifest" / "recovery", ec)) {
        if (ParseFiles::is_json_file(dirit.path())) {
            auto manifests = ParseFiles::parse<ParseFiles::RecoveryManifest>(
                dirit.path(), deviceMap, romBranchMap);
            parsedManifests.insert(parsedManifests.end(), manifests.begin(),
                                   manifests.end());
            DLOG(INFO) << "Recovery Add file: " << dirit.path().filename()
                       << ": " << manifests.size();
        }
    }
    if (ec) {
        LOG(ERROR) << "Error scanning recovery directory: " << ec.message();
        return false;
    }

    // Clear the old data (refcounts of shared objects)
    devices.clear();
    roms.clear();
    recoveries.clear();

    // Now, clean the unreferenced device/roms
    for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
        if (it->second.unique()) {
            DLOG(INFO) << "Removing device " << it->second->toString();
            it = deviceMap.erase(it);
        }
    }
    for (auto it = romBranchMap.begin(); it != romBranchMap.end(); ++it) {
        if (it->second.unique()) {
            DLOG(INFO) << "Removing rom " << it->second->toString();
            it = romBranchMap.erase(it);
        }
    }
    // Return the localManifests vector
    return !parsedManifests.empty();
}

namespace {
bool zipFilePrefixer(std::string_view filename, std::string_view prefix,
                     const bool debug) {
    bool startsWith = filename.starts_with(prefix);
    bool endsWith = filename.ends_with(".zip");
    if (debug) {
        LOG(INFO) << fmt::format(
            "ZipFilePrefixer: StartsWith '{}': {}, "
            "endsWithZip: {}, entry: {}",
            prefix, startsWith, endsWith, filename);
    }
    return startsWith && endsWith;
}

bool exactMatcher(std::string_view filename, std::string_view prefix,
                  const bool debug) {
    if (debug) {
        LOG(INFO) << fmt::format("ExactMatcher: Comparing {} with {}", filename,
                                 prefix);
    }
    return filename == prefix;
}

bool containsMatcher(std::string_view filename, std::string_view prefix,
                     const bool debug) {
    if (debug) {
        LOG(INFO) << fmt::format(
            "ContainsMatcher: Checking if '{}' contains '{}'", filename,
            prefix);
    }
    return filename.find(prefix) != std::string::npos;
}
}  // namespace

ConfigParser::ConfigParser(std::filesystem::path jsonFileDir)
    : _jsonFileDir(std::move(jsonFileDir)) {
    // Add default matchers
    storage.add("ZipFilePrefixer", zipFilePrefixer);
    storage.add("ExactMatcher", exactMatcher);
    storage.add("ContainsMatcher", containsMatcher);

    // Load files
    LOG(INFO) << "Loading JSON files from directory: " << _jsonFileDir;
    merge();
}

std::vector<ConfigParser::LocalManifest::Ptr> ConfigParser::manifests() const {
    return parsedManifests;
}

bool ConfigParser::LocalManifest::GitPrepare::prepare(
    const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        info.git_clone(path);
    } else {
        LOG(INFO) << "Local manifest exists already...";
        GitBranchSwitcher sl{path};
        if (sl.open() && sl.checkout(info)) {
            LOG(INFO) << "Repo is up-to-date.";
        } else {
            LOG(WARNING)
                << "Local manifest is not the correct repository, deleting it.";
            std::filesystem::remove_all(path);
            info.git_clone(path);
        }
    }
    return std::filesystem::exists(path);
}

#include <libxml/tree.h>

bool ConfigParser::LocalManifest::WritePrepare::prepare(
    const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }
    auto* doc = xmlNewDoc(BAD_CAST "1.0");
    if (doc == nullptr) {
        LOG(ERROR) << "Failed to create XML document";
        return false;
    }
    auto* root = xmlNewNode(nullptr, BAD_CAST "manifest");
    xmlDocSetRootElement(doc, root);
    auto* comment =
        xmlNewDocComment(doc, BAD_CAST "Auto generated by c_cpp_telegrambot");
    xmlAddChild(root, comment);

    // Create a remote in local manifest
    constexpr std::string_view kGithubRemoteName = "cppbot_github";
    constexpr std::string_view kGithubUrl = "https://github.com/";
    xmlNodePtr remote = xmlNewChild(root, nullptr, BAD_CAST "remote", nullptr);
    xmlNewProp(remote, BAD_CAST "name", BAD_CAST kGithubRemoteName.data());
    xmlNewProp(remote, BAD_CAST "fetch", BAD_CAST kGithubUrl.data());

    // Create repo entries.
    for (const auto& repo : data) {
        xmlNodePtr repoNode =
            xmlNewChild(root, nullptr, BAD_CAST "project", nullptr);
        auto name = absl::StripPrefix(repo.url(), kGithubUrl);
        xmlNewProp(repoNode, BAD_CAST "name", BAD_CAST name.data());
        xmlNewProp(repoNode, BAD_CAST "path",
                   BAD_CAST repo.destination.c_str());
        xmlNewProp(repoNode, BAD_CAST "remote",
                   BAD_CAST kGithubRemoteName.data());
        xmlNewProp(repoNode, BAD_CAST "clone-depth", BAD_CAST "1");
        xmlNewProp(repoNode, BAD_CAST "revision",
                   BAD_CAST repo.branch().c_str());
    }

    // Save the XML file
    std::string xmlFilePath = path / "local_manifest.xml";
    xmlSaveFormatFileEnc(xmlFilePath.c_str(), doc, "UTF-8", 1);
    xmlFreeDoc(doc);
    return true;
}
