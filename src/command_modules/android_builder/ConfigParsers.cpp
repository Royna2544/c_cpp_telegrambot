#include "ConfigParsers.hpp"

#include <absl/strings/strip.h>
#include <fmt/format.h>
#include <json/json.h>

#include <algorithm>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "SystemInfo.hpp"

class ConfigParser::Parser {
    Json::Value root;

    // Parse 'roms' section
    std::vector<ROMBranch::Ptr> parseROMManifest();
    // Parse 'targets' section
    std::vector<Device::Ptr> parseDevices();
    // Parse 'local_manifests' section
    std::vector<LocalManifest::Ptr> parseLocalManifestBranch();
    // Merge parsed data to create a linked list of LocalManifests
    bool merge();

    std::vector<LocalManifest::Ptr> mergedManifests;

    // Artifact matcher
    std::unordered_map<std::string, ArtifactMatcher::Ptr> artifactMatchers;
    void addArtifactMatcher(const std::string& name,
                            const ArtifactMatcher::MatcherType& fn) {
        artifactMatchers[name] = std::make_shared<ArtifactMatcher>(fn, name);
    }

   public:
    explicit Parser(const std::filesystem::path& jsonFileDir);
    [[nodiscard]] std::vector<LocalManifest::Ptr> parse();
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
                "Expected object, found: (Json::ValueType){} while accessing "
                "property '{}'",
                static_cast<int>(root_.type()), name);
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

    const auto lambda = [this, &romBranches](
                            const Json::Value& entry,
                            const std::string& default_target) {
        auto romInfo = std::make_shared<ROMInfo>();
        auto ROMName = "name"_s;
        auto ROMLink = "link"_s;
        auto ROMTarget = "target"_s.setDefaultValue(default_target);

        if (!checkRequirements(entry, ROMName, ROMLink, ROMTarget)) {
            LOG(INFO) << "Skipping invalid roms entry: "
                      << entry.toStyledString();
            return;
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
                return;
            }
            if (!artifactMatchers.contains(matcher)) {
                LOG(WARNING) << "No matching artifact matcher found for: "
                             << static_cast<std::string>(matcher);
                return;
            }
            romInfo->artifact = artifactMatchers[matcher];
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
            auto rombranch = std::make_shared<ROMBranch>();
            rombranch->branch = branch;
            rombranch->androidVersion = androidVersion;
            rombranch->romInfo = romInfo;
            romBranches.emplace_back(rombranch);
        }
    };
    for (const auto& entry : ProxyJsonBranch(root, "roms")) {
        lambda(entry, "bacon");
    }
    for (const auto& entry : ProxyJsonBranch(root, "recoveries")) {
        lambda(entry, "recoveryimage");
    }
    return romBranches;
}

std::vector<ConfigParser::Device::Ptr> ConfigParser::Parser::parseDevices() {
    std::vector<Device::Ptr> devices;

    for (const auto& entry : ProxyJsonBranch(root, "targets")) {
        auto codename = "codename"_s;
        auto name = "name"_s;

        if (!checkRequirements(entry, codename, name)) {
            LOG(INFO) << "Skipping invalid target entry: "
                      << entry.toStyledString();
            continue;
        }
        auto device = std::make_shared<Device>();
        device->codename = codename;
        device->marketName = name;
        devices.emplace_back(device);
        DLOG(INFO) << "Parsed device: " << device->codename;
    }
    return devices;
}

std::vector<ConfigParser::LocalManifest::Ptr>
ConfigParser::Parser::parseLocalManifestBranch() {
    std::vector<LocalManifest::Ptr> localManifests;

    for (const auto& entry : ProxyJsonBranch(root, "local_manifests")) {
        auto name = "name"_s;
        auto manifest = "url"_s;
        if (!checkRequirements(entry, name, manifest)) {
            LOG(INFO) << "Skipping invalid localmanifest entry: "
                      << entry.toStyledString();
            continue;
        }
        for (const auto& manifestEntry : ProxyJsonBranch(entry, "branches")) {
            auto localManifest = std::make_shared<LocalManifest>();
            auto branchName = "name"_s;
            auto target_rom = "target_rom"_s;
            auto android_version = "android_version"_d;
            auto device = "device"_s.setRequired(false);
            std::vector<std::string> deviceCodenames;
            if (!checkRequirements(manifestEntry, name, target_rom, branchName,
                                   android_version, device)) {
                LOG(INFO) << "Skipping invalid localmanifest-branches entry: "
                          << entry.toStyledString();
                continue;
            }
            if (device.hasValue()) {
                deviceCodenames.emplace_back(device);
            } else {
                for (const auto& deviceEntry :
                     ProxyJsonBranch(manifestEntry, "devices")) {
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
            localManifest->prepare =
                std::make_shared<LocalManifest::GitPrepare>(
                    RepoInfo{manifest, branchName});
            localManifest->rom = lookup;
            {
                static int job_count = 0;
                static std::once_flag once;
                std::call_once(once, [&]() {
                    const auto totalMem =
                        MemoryInfo().totalMemory.to<SizeTypes::GigaBytes>();
                    LOG(INFO) << "Total memory: " << totalMem;
                    job_count = static_cast<int>(totalMem / 4 - 1);
                    LOG(INFO)
                        << "Using job count: " << job_count << " for ROM build";
                });
                localManifest->job_count = job_count;
            }
            localManifests.emplace_back(localManifest);
            DLOG(INFO) << "Parsed local manifest: " << localManifest->name;
        }
    }

    for (const auto& entry : root["recovery_targets"]) {
        auto localManifest = std::make_shared<LocalManifest>();
        auto name = "name"_s;
        auto android_version = "android_version"_d;
        auto device = "device"_s.setRequired(false);
        auto recoveryName = "target_recovery"_s;
        std::vector<std::string> deviceCodenames;

        if (!checkRequirements(entry, name, android_version, device,
                               recoveryName)) {
            LOG(INFO) << "Skipping invalid recovery_targets entry: "
                      << entry.toStyledString();
            continue;
        }
        if (device.hasValue()) {
            deviceCodenames.emplace_back(device);
        } else {
            for (const auto& deviceEntry : ProxyJsonBranch(entry, "devices")) {
                deviceCodenames.emplace_back(deviceEntry.asString());
            }
        }
        if (deviceCodenames.empty()) {
            LOG(WARNING) << "No device specified for recovery_targets: "
                         << (std::string)name;
        }
        auto prepare = std::make_shared<LocalManifest::WritePrepare>();
        for (const auto& mapping : ProxyJsonBranch(entry, "clone_mapping")) {
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
        LocalManifest::ROMLookup lookup;
        lookup.name = recoveryName;
        lookup.androidVersion = android_version;
        localManifest->name = name;
        localManifest->prepare = prepare;
        localManifest->rom = lookup;
        localManifest->devices = std::move(deviceCodenames);
        localManifest->job_count = std::thread::hardware_concurrency();
        localManifests.emplace_back(localManifest);
        DLOG(INFO) << "Parsed local manifest: " << localManifest->name;
    }
    return localManifests;
}

bool ConfigParser::Parser::merge() {
    // Parse all the components
    auto romBranches = parseROMManifest();
    auto devices = parseDevices();
    auto localManifests = parseLocalManifestBranch();

    std::unordered_map<std::string, Device::Ptr> deviceMap;
    for (const auto& device : devices) {
        deviceMap[device->codename] = device;
    }
    decltype(localManifests) additionalLocalManifests;

    // Link local manifests to ROM branches and devices
    for (const auto& localManifest : localManifests) {
        const auto romLookup =
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
                    << "Device info not found for codename: " << codename;
                auto obj = std::make_shared<Device>(codename);

                deviceMap[codename] = obj;
                deviceList.emplace_back(obj);
            }
        }
        std::ranges::sort(deviceList,
                          [](const Device::Ptr& a, const Device::Ptr& b) {
                              return a->codename < b->codename;
                          });
        localManifest->devices = deviceList;

        // Find corresponding ROM branch
        bool foundRomBranch = false;
        // If "*" is specified, link all ROM branches with the same android
        // version
        if (romLookup.name == "*") {
            ROMBranch::Ptr firstBranch = nullptr;
            for (const auto& branchPtr : romBranches) {
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
            for (const auto& branchPtr : romBranches) {
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
    // Add default matchers
    addArtifactMatcher("ZipFilePrefixer", [](std::string_view filename,
                                             std::string_view prefix) {
        return filename.starts_with(prefix) && filename.ends_with(".zip");
    });
    addArtifactMatcher("ExactMatcher",
                       [](std::string_view filename, std::string_view prefix) {
                           return filename == prefix;
                       });
    // Load and parse file
    std::ifstream file(jsonFileDir);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open JSON file: " +
                                 jsonFileDir.string());
    }
    file >> root;
    LOG(INFO) << "JSON file loaded successfully";
}

ConfigParser::ConfigParser(const std::filesystem::path& jsonFileDir) {
    // Load files
    DLOG(INFO) << "Loading JSON files from directory: " << jsonFileDir;
    for (const auto& entry : std::filesystem::directory_iterator(jsonFileDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            DLOG(INFO) << "Parsing JSON file: " << entry.path().filename();
            try {
                Parser parser(entry.path());
                const auto parsed = parser.parse();
                if (parsed.size() == 0) {
                    LOG(WARNING) << "No manifests parsed";
                    continue;
                }
                parsedManifests.insert(parsedManifests.end(), parsed.begin(),
                                       parsed.end());
                LOG(INFO) << fmt::format(
                    "Adding {} manifests to the global set.", parsed.size());
            } catch (const std::exception& e) {
                LOG(ERROR) << "Error parsing JSON file: "
                           << entry.path().string() << ", error: " << e.what();
                throw;
            }
        }
    }
    LOG(INFO) << fmt::format("Parsed total of {} manifests",
                             parsedManifests.size());
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
        if (*rom == *romBranch &&
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
    LOG(WARNING) << fmt::format("ROM branch not found: {}, Android {}", romName,
                                androidVersion);
    return nullptr;
}

bool ConfigParser::LocalManifest::GitPrepare::operator()(
    const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        GitUtils::git_clone(info, path);
    } else {
        LOG(INFO) << "Local manifest exists already...";
        GitBranchSwitcher switcherLocal{
            .gitDirectory = path,
            .desiredBranch = info.branch,
            .desiredUrl = info.url,
            .checkout = true,
        };
        if (switcherLocal()) {
            LOG(INFO) << "Repo is up-to-date.";
        } else {
            LOG(WARNING)
                << "Local manifest is not the correct repository, deleting it.";
            std::filesystem::remove_all(path);
            GitUtils::git_clone(info, path);
        }
    }
    return std::filesystem::exists(path);
}

#include <libxml/tree.h>

bool ConfigParser::LocalManifest::WritePrepare::operator()(
    const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }
    auto* doc = xmlNewDoc(BAD_CAST "1.0");
    if (!doc) {
        LOG(ERROR) << "Failed to create XML document";
        return false;
    }
    auto* root = xmlNewNode(nullptr, BAD_CAST "manifest");
    xmlDocSetRootElement(doc, root);
    auto* comment = xmlNewDocComment(doc, BAD_CAST "Auto generated by c_cpp_telegrambot");
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
        auto name = absl::StripPrefix(repo.url, kGithubUrl);
        xmlNewProp(repoNode, BAD_CAST "name", BAD_CAST name.data());
        xmlNewProp(repoNode, BAD_CAST "path",
                   BAD_CAST repo.destination.c_str());
        xmlNewProp(repoNode, BAD_CAST "remote",
                   BAD_CAST kGithubRemoteName.data());
        xmlNewProp(repoNode, BAD_CAST "clone-depth", BAD_CAST "1");
        xmlNewProp(repoNode, BAD_CAST "revision", BAD_CAST repo.branch.c_str());
    }

    // Save the XML file
    std::string xmlFilePath = path / "local_manifest.xml";
    xmlSaveFormatFileEnc(xmlFilePath.c_str(), doc, "UTF-8", 1);
    xmlFreeDoc(doc);
    return true;
}