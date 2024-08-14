#include "ConfigParsers.hpp"

#include <json/value.h>

#include <algorithm>
#include <fstream>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

// Repesent a type and name string
template <typename T>
struct NodeItemType {
    using type = std::decay_t<T>;

   private:
    std::string name;
    bool required = true;
    T value_;
    bool hasDefaultValue = false;

   public:
    NodeItemType(std::string name) : name(std::move(name)) {}
    NodeItemType& setRequired(bool required) {
        this->required = required;
        return *this;
    }
    NodeItemType& setDefaultValue(const T& defaultValue) {
        static_assert(std::is_same_v<T, std::decay_t<T>>,
                      "Default value type must match node type");
        static_assert(!std::is_const_v<T>,
                      "Default value cannot be a const type");
        value_ = defaultValue;
        hasDefaultValue = true;
        return *this;
    }

    NodeItemType& setValue(const T& value) {
        static_assert(std::is_same_v<T, std::decay_t<T>>,
                      "Value type must match node type");
        value_ = value;
        return *this;
    }
    operator T() const { return value_; }
    [[nodiscard]] bool hasValue() const {
        return hasDefaultValue || value_ != T{};
    }

    template <typename... Args>
    friend bool checkRequirements(const Json::Value& value, Args&... args);
};

// Check if T is Specialization of NodeNameType
template <typename T>
struct IsNodeNameType : std::false_type {};
template <typename T>
struct IsNodeNameType<NodeItemType<T>> : std::true_type {};

NodeItemType<std::string> operator""_s(const char* str,
                                       unsigned long /*unused*/) {
    return {str};
}
NodeItemType<int> operator""_d(const char* str, unsigned long /*unused*/) {
    return {str};
}

template <typename... Args>
bool checkRequirements(const Json::Value& value, Args&... args) {
    static_assert(sizeof...(args) > 0,
                  "At least one argument required for checkRequirements");

    static_assert((... && IsNodeNameType<Args>::value),
                  "All arguments must be NodeItemType");

    if (!value.isObject()) {
        LOG(ERROR) << "Expected object, found: " << value.type();
        return false;
    }

    bool allRequiredMet = true;
    (([&allRequiredMet, value, &args] {
         if (!value.isMember(args.name)) {
             if (args.hasDefaultValue) {
                 LOG(INFO) << "Using default value for field: " << args.name;
             }
             if (args.required) {
                 allRequiredMet = false;
             }
             LOG(WARNING) << "Missing field: " << args.name
                          << ", required: " << std::boolalpha << args.required;
             if (!allRequiredMet) {
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
    explicit ProxyJsonBranch(Json::Value root)
        : root_(std::move(root)), isValidObject(root_.isArray()) {
        if (!isValidObject) {
            LOG(ERROR) << "Expected object, found: (Json::ValueType)"
                       << root_.type();
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

std::vector<ConfigParser::ROMBranch::Ptr>
ConfigParser::Parser::parseROMManifest() {
    std::vector<ROMBranch::Ptr> romBranches;

    for (const auto& entry : ProxyJsonBranch(root["roms"])) {
        auto romInfo = std::make_shared<ROMInfo>();
        auto ROMName = "name"_s;
        auto ROMLink = "link"_s;
        auto ROMTarget = "target"_s.setDefaultValue("bacon");
        auto OutZipPrefix = "outzip_prefix"_s;

        if (!checkRequirements(entry, ROMName, ROMLink, ROMTarget,
                               OutZipPrefix)) {
            continue;
        }

        romInfo->name = ROMName;
        romInfo->url = ROMLink;
        romInfo->target = ROMTarget;
        romInfo->prefixOfOutput = OutZipPrefix;

        for (const auto& branchEntry : ProxyJsonBranch(entry["branches"])) {
            auto androidVersion = "android_version"_d;
            auto branch = "branch"_s;
            if (!checkRequirements(branchEntry, androidVersion, branch)) {
                continue;
            }
            auto rombranch = std::make_shared<ROMBranch>();
            rombranch->branch = branch;
            rombranch->androidVersion = androidVersion;
            rombranch->romInfo = romInfo;
            romBranches.emplace_back(rombranch);
        }
    }
    return romBranches;
}

std::vector<ConfigParser::Device::Ptr> ConfigParser::Parser::parseDevices() {
    std::vector<Device::Ptr> devices;

    for (const auto& entry : ProxyJsonBranch(root["targets"])) {
        auto codename = "codename"_s;
        auto name = "name"_s;

        if (!checkRequirements(entry, codename, name)) {
            continue;
        }
        auto device = std::make_shared<Device>();
        device->codename = codename;
        device->marketName = name;
        devices.emplace_back(device);
    }
    return devices;
}

std::vector<ConfigParser::LocalManifest::Ptr>
ConfigParser::Parser::parseLocalManifestBranch() {
    std::vector<LocalManifest::Ptr> localManifests;

    for (const auto& entry : ProxyJsonBranch(root["local_manifests"])) {
        auto name = "name"_s;
        auto manifest = "url"_s;
        if (!checkRequirements(entry, name, manifest)) {
            continue;
        }
        for (const auto& manifestEntry : ProxyJsonBranch(entry["branches"])) {
            auto localManifest = std::make_shared<LocalManifest>();
            auto branchName = "name"_s;
            auto target_rom = "target_rom"_s;
            auto android_version = "android_version"_d;
            auto device = "device"_s.setRequired(false);
            std::vector<std::string> deviceCodenames;
            if (!checkRequirements(manifestEntry, name, target_rom, branchName,
                                   android_version, device)) {
                continue;
            }
            if (device.hasValue()) {
                deviceCodenames.emplace_back(device);
            } else {
                for (const auto& deviceEntry :
                     ProxyJsonBranch(manifestEntry["devices"])) {
                    deviceCodenames.emplace_back(deviceEntry.asString());
                }
            }
            if (deviceCodenames.empty()) {
                LOG(WARNING) << "No device specified for local manifest: "
                             << (std::string)name;
            }
            LocalManifest::ROMLookup lookup;
            localManifest->devices = deviceCodenames;
            lookup.name = target_rom;
            lookup.androidVersion = android_version;
            localManifest->name = name;
            localManifest->repo_info.url = manifest;
            localManifest->repo_info.branch = branchName;
            localManifest->rom = lookup;
            localManifests.emplace_back(localManifest);
        }
    }
    return localManifests;
}

bool ConfigParser::Parser::merge() {
    // Parse all the components
    auto romBranches = parseROMManifest();
    auto devices = parseDevices();
    auto localManifests = parseLocalManifestBranch();

    // Create maps for quick lookup
    std::unordered_map<std::string, ROMBranch::Ptr> romBranchMap;
    for (const auto& branch : romBranches) {
        romBranchMap[branch->branch] = branch;
    }

    std::unordered_map<std::string, Device::Ptr> deviceMap;
    for (const auto& device : devices) {
        deviceMap[device->codename] = device;
    }
    decltype(localManifests) additionalLocalManifests;

    // Link local manifests to ROM branches and devices
    for (const auto& localManifest : localManifests) {
        const auto& romLookup =
            std::get<LocalManifest::ROMLookup>(localManifest->rom);

        // Link devices to local manifest
        std::vector<Device::Ptr> deviceList;
        const auto& deviceNames = std::get<0>(localManifest->devices);
        for (const auto& codename : deviceNames) {
            auto deviceIt = deviceMap.find(codename);
            if (deviceIt != deviceMap.end()) {
                deviceList.emplace_back(deviceIt->second);
            } else {
                LOG(WARNING)
                    << "Device info not found for codename: " << codename
                    << ", create dummy";
                deviceList.emplace_back(std::make_shared<Device>(codename));
            }
        }
        localManifest->devices = deviceList;

        // Find corresponding ROM branch
        bool foundRomBranch = false;
        // If "*" is specified, link all ROM branches with the same android
        // version
        if (romLookup.name == "*") {
            ROMBranch::Ptr firstBranch = nullptr;
            for (const auto& [branchName, branchPtr] : romBranchMap) {
                if (branchPtr->androidVersion == romLookup.androidVersion) {
                    if (!firstBranch) {
                        foundRomBranch = true;
                        firstBranch = branchPtr;
                    } else {
                        localManifest->rom = branchPtr;
                        additionalLocalManifests.emplace_back(
                            std::make_shared<LocalManifest>(*localManifest));
                    }
                }
            }
            if (!foundRomBranch) {
                LOG(ERROR) << "No ROM branch found for android version: "
                           << romLookup.androidVersion;
                continue;
            }
            localManifest->rom = firstBranch;
        } else {
            for (const auto& [branchName, branchPtr] : romBranchMap) {
                if (branchPtr->romInfo->name == romLookup.name &&
                    branchPtr->androidVersion == romLookup.androidVersion) {
                    localManifest->rom = branchPtr;
                    foundRomBranch = true;
                    break;
                }
            }
        }

        if (!foundRomBranch) {
            LOG(ERROR) << "ROM branch not found for local manifest: "
                       << romLookup.name << " A" << romLookup.androidVersion;
            continue;
        }
    }
    // Merge additional local manifests with the existing ones
    mergedManifests.insert(mergedManifests.end(),
                           additionalLocalManifests.begin(),
                           additionalLocalManifests.end());

    // Final verification to ensure all required components are linked
    bool allLinked = true;
    for (const auto& localManifest : localManifests) {
        if (!std::holds_alternative<ROMBranch::Ptr>(localManifest->rom)) {
            LOG(ERROR) << "Local manifest missing ROM branch linkage: "
                       << localManifest->name;
            allLinked = false;
        }
        if (getValue(localManifest->devices).empty()) {
            LOG(WARNING) << "Local manifest has no linked devices: "
                         << localManifest->name;
        }
    }
    if (allLinked) {
        mergedManifests.insert(mergedManifests.end(), localManifests.begin(),
                               localManifests.end());
    }

    // Return the localManifests vector
    return allLinked;
}

std::vector<ConfigParser::LocalManifest::Ptr> ConfigParser::Parser::parse() {
    if (!merge()) {
        throw std::runtime_error("Failed to merge components");
    }
    return mergedManifests;
}

ConfigParser::Parser::Parser(const std::filesystem::path& jsonFileDir) {
    std::ifstream file(jsonFileDir);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open JSON file: " +
                                 jsonFileDir.string());
    }
    file >> root;
    LOG(INFO) << "JSON file loaded successfully";
}

ConfigParser::ConfigParser(const std::filesystem::path& jsonFileDir) {
    DLOG(INFO) << "Loading JSON files from directory: " << jsonFileDir;
    for (const auto& entry : std::filesystem::directory_iterator(jsonFileDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            DLOG(INFO) << "Parsing JSON file: " << entry.path().filename();
            try {
                Parser parser(entry.path());
                const auto parsed = parser.parse();
                parsedManifests.insert(parsedManifests.end(), parsed.begin(),
                                       parsed.end());
            } catch (const std::exception& e) {
                LOG(ERROR) << "Error parsing JSON file: "
                           << entry.path().string() << ", error: " << e.what();
                throw;
            }
        }
    }
}

std::set<ConfigParser::Device::Ptr> ConfigParser::getDevices() const {
    std::unordered_map<std::string, Device::Ptr> devices;
    for (const auto& manifest : parsedManifests) {
        const auto& deviceList = getValue(manifest->devices);
        for (const auto& device : deviceList) {
            devices[device->codename] = device;
        }
    }
    std::set<Device::Ptr> deviceSet;
    for (const auto& [codename, device] : devices) {
        deviceSet.insert(device);
    }
    return deviceSet;
}

std::set<ConfigParser::ROMBranch::Ptr> ConfigParser::getROMBranches(
    const Device::Ptr& device) const {
    std::set<ROMBranch::Ptr> romBranches;
    for (const auto& manifest : parsedManifests) {
        const auto& rom = getValue(manifest->rom);
        const auto& deviceList = getValue(manifest->devices);
        if (std::ranges::any_of(
                deviceList, [&device](const auto& d) { return d == device; })) {
            romBranches.insert(rom);
        }
    }
    return romBranches;
}

ConfigParser::LocalManifest::Ptr ConfigParser::getLocalManifest(
    const ROMBranch::Ptr& romBranch, const Device::Ptr& device) const {
    for (const auto& manifest : parsedManifests) {
        const auto& rom = getValue(manifest->rom);
        const auto& deviceList = getValue(manifest->devices);
        if (rom == romBranch &&
            std::ranges::any_of(
                deviceList, [&device](const auto& d) { return d == device; })) {
            return manifest;
        }
    }
    LOG(WARNING) << "Local manifest not found for ROM branch: "
                 << romBranch->toString() << ", device: " << device->codename;
    return nullptr;
}

ConfigParser::Device::Ptr ConfigParser::getDevice(
    const std::string_view& codename) const {
    for (const auto& device : getDevices()) {
        if (device->codename == codename) {
            return device;
        }
    }
    return nullptr;
}

ConfigParser::ROMBranch::Ptr ConfigParser::getROMBranches(
    const std::string& romName, const int androidVersion) const {
    for (const auto& localManifest : parsedManifests) {
        const auto& rom = getValue(localManifest->rom);
        if (rom->romInfo->name == romName &&
            rom->androidVersion == androidVersion) {
            return rom;
        }
    }
    LOG(WARNING) << "ROM branch not found: " << romName << " A"
                 << androidVersion;
    return nullptr;
}