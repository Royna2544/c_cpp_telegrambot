#include "ConfigParsers.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

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
    friend bool checkRequirements(const nlohmann::json& value,
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
bool checkRequirements(const nlohmann::json& value, NodeItemType<Args>&... args) {
    static_assert(sizeof...(args) > 0,
                  "At least one argument required for checkRequirements");

    if (!value.is_object()) {
        LOG(ERROR) << fmt::format("Expected object, found: {}",
                                  value.type_name());
        return false;
    }

    bool allRequiredMet = true;
    (([&allRequiredMet, &value, &args] {
         if (!value.contains(args.name)) {
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
                 if (!node.is_number_integer()) {
                     LOG(ERROR) << "Expected integer for field: " << args.name;
                     allRequiredMet = false;
                 } else {
                     args.setValue(node.get<int>());
                 }
             }
             if constexpr (std::is_same_v<
                               typename std::decay_t<decltype(args)>::type,
                               std::string>) {
                 if (!node.is_string()) {
                     LOG(ERROR) << "Expected string for field: " << args.name;
                     allRequiredMet = false;
                 } else {
                     args.setValue(node.get<std::string>());
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
 * This class wraps a nlohmann::json object and provides iterator access to its
 * members. It also performs basic validation to ensure that the root value is
 * an object.
 *
 * @param root The nlohmann::json object to be wrapped.
 */
class ProxyJsonBranch {
   public:
    /**
     * @brief Returns an iterator pointing to the beginning of the wrapped JSON
     * object.
     *
     * @return An iterator pointing to the beginning of the wrapped JSON object.
     */
    [[nodiscard]] nlohmann::json::const_iterator begin() const {
        return isValidObject ? root_.begin() : root_.end();
    }

    /**
     * @brief Returns an iterator pointing to the end of the wrapped JSON
     * object.
     *
     * @return An iterator pointing to the end of the wrapped JSON object.
     */
    [[nodiscard]] nlohmann::json::const_iterator end() const {
        return root_.end();
    }

    /**
     * @brief Constructor for ProxyJsonBranch.
     *
     * Constructs a ProxyJsonBranch object and performs basic validation on the
     * provided nlohmann::json object. If the root value is not an array, it logs
     * an error message.
     *
     * @param root The nlohmann::json object to be wrapped.
     */
    explicit ProxyJsonBranch(const nlohmann::json& root, const std::string& name)
        : root_(root.contains(name) && root[name].is_array() ? root[name] : nlohmann::json::array()), 
          isValidObject(root.contains(name) && root[name].is_array()) {
        if (!isValidObject) {
            if (!root.contains(name)) {
                LOG(WARNING) << "Property '" << name << "' does not exist in JSON object";
            } else {
                LOG(ERROR) << fmt::format(
                    "Expected array for property '{}', but got: {}",
                    name, root[name].type_name());
            }
        }
    }

   private:
    nlohmann::json root_;
    bool isValidObject;
};

namespace ParseFiles {

std::vector<ConfigParser::ROMBranch::Ptr> parseROM(
    const nlohmann::json& entry, const std::string& default_target,
    ConfigParser::MatcherStorage* storage) {
    auto romInfo = std::make_shared<ConfigParser::ROMInfo>();
    auto ROMName = "name"_s;
    auto ROMLink = "link"_s;
    auto ROMTarget = "target"_s.setDefaultValue(default_target);
    std::vector<ConfigParser::ROMBranch::Ptr> romBranches;

    if (!checkRequirements(entry, ROMName, ROMLink, ROMTarget)) {
        LOG(INFO) << "Skipping invalid roms entry: " << entry.dump();
        return {};
    }
    if (!entry.contains("artifact")) {
        LOG(INFO) << fmt::format("Artifact entry not found for '{}'.",
                                 static_cast<std::string>(ROMName));
    } else {
        auto matcher = "matcher"_s;
        auto data = "data"_s;
        if (!checkRequirements(entry["artifact"], matcher, data)) {
            LOG(INFO) << "Skipping invalid artifact entry: "
                      << entry.dump();
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
                      << entry.dump();
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
    static constexpr auto _value = nlohmann::json::value_t::array;
    static std::vector<return_type> parse(
        const nlohmann::json& root, ConfigParser::MatcherStorage* storage) {
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
    static constexpr auto _value = nlohmann::json::value_t::array;
    static std::vector<return_type> parse(
        const nlohmann::json& root, ConfigParser::MatcherStorage* storage) {
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

std::pair<Devices, ConfigParser::ROMBranch::Ptr> parseDeviceAndRom(
    const NodeItemType<std::string>& device, const nlohmann::json& rootNode,
    const DeviceMap& devicesPtr, const ROMsMap& romsPtr,
    const std::string_view target_rom, const int android_version) {
    ConfigParser::ROMBranch::Ptr romBranch;
    std::vector<std::string> deviceCodenames;

    // Parse codenames array
    if (device.hasValue()) {
        deviceCodenames.emplace_back(device);
    } else {
        for (const auto& deviceEntry : ProxyJsonBranch(rootNode, "devices")) {
            deviceCodenames.emplace_back(deviceEntry.get<std::string>());
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
        romBranch = romsPtr.at(key);
    } else {
        LOG(WARNING) << "No matching ROM for target_rom " << target_rom
                     << " and android_version " << android_version;
        return {};
    }
    return std::make_pair(std::move(devicePtrs), std::move(romBranch));
}

struct ROMLocalManifest {
    using return_type = ConfigParser::LocalManifest::Ptr;
    static constexpr auto _value = nlohmann::json::value_t::object;
    static std::vector<return_type> parseOneLocalManifest(
        const nlohmann::json& json, const DeviceMap& devices, const ROMsMap& roms,
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
                      << json.dump();
            return {};
        }

        auto [devicesVec, rom] = parseDeviceAndRom(
            device, json, devices, roms, static_cast<std::string>(target_rom),
            android_version);

        if (devicesVec.empty() || !rom) {
            LOG(INFO) << "No matching device or ROM for localmanifest-branches "
                      << json.dump();
            return {};
        }
        // Assign to the localmanifest
        localManifest.devices = devicesVec;
        localManifest.name = std::move(name);
        localManifest.preparar = ConfigParser::LocalManifest::GitPrepare(
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
        localManifest.rom = rom;
        result.emplace_back(std::make_shared<ConfigParser::LocalManifest>(
            std::move(localManifest)));
        return result;
    }
    static std::vector<return_type> parse(const nlohmann::json& root,
                                          const DeviceMap& devices,
                                          const ROMsMap& roms) {
        auto name = "name"_s;
        auto manifest = "url"_s;
        if (!checkRequirements(root, name, manifest)) {
            LOG(INFO) << "Skipping invalid localmanifest entry: "
                      << root.dump();
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
    static constexpr auto _value = nlohmann::json::value_t::object;
    static std::vector<return_type> parse(const nlohmann::json& root,
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
                      << root.dump();
            return {};
        }

        auto [devicesVec, rom] = parseDeviceAndRom(
            device, root, devices, roms, static_cast<std::string>(recoveryName),
            android_version);
        if (devicesVec.empty() || !rom) {
            LOG(INFO) << "No matching device or ROM for localmanifest-branches "
                      << root.dump();
            return {};
        }

        ConfigParser::LocalManifest::WritePrepare prepare;
        for (const auto& mapping : ProxyJsonBranch(root, "clone_mapping")) {
            auto link = "link"_s;
            auto branch = "branch"_s;
            auto destination = "destination"_s;
            if (!checkRequirements(mapping, link, branch, destination)) {
                LOG(INFO) << "Skipping invalid clone_mapping entry: "
                          << mapping.dump();
                continue;
            }
            prepare.data.emplace_back(link, branch,
                                      std::filesystem::path(destination));
        }
        localManifest.name = name;
        localManifest.preparar = std::move(prepare);
        localManifest.devices = std::move(devicesVec);
        localManifest.job_count = std::thread::hardware_concurrency();
        localManifest.rom = rom;
        manifests.emplace_back(std::make_shared<ConfigParser::LocalManifest>(
            std::move(localManifest)));

        return manifests;
    }
};

struct DeviceNames {
    using return_type = ConfigParser::Device::Ptr;
    static constexpr auto _value = nlohmann::json::value_t::array;
    static std::vector<return_type> parse(const nlohmann::json& root) {
        std::vector<return_type> devices;
        for (const auto& entry : root) {
            auto codename = "codename"_s;
            auto name = "name"_s;

            if (!checkRequirements(entry, codename, name)) {
                LOG(INFO) << "Skipping invalid target entry: "
                          << entry.dump();
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
    nlohmann::json value;

    std::ifstream file(jsonFile);
    if (!file.is_open()) {
        LOG(ERROR) << fmt::format("Failed to open {}",
                                  jsonFile.filename().string());
        return {};
    }

    try {
        file >> value;
    } catch (const nlohmann::json::parse_error& e) {
        LOG(ERROR) << fmt::format("Failed to parse {}: {}",
                                  jsonFile.filename().string(), e.what());
        return {};
    }
    if (value.type() != T::_value) {
        LOG(ERROR) << fmt::format(
            "Expected {} for {} (actual {})", 
            nlohmann::json(T::_value).type_name(),
            jsonFile.filename().string(), 
            value.type_name());
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
    std::erase_if(deviceMap,
                  [](const auto& pair) { return pair.second.unique(); });
    std::erase_if(romBranchMap,
                  [](const auto& pair) { return pair.second.unique(); });

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
