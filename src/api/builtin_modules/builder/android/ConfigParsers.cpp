#include "ConfigParsers.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <regex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ParseFiles {

// roms.json
struct ROM {
    std::string name;
    std::string link;
    std::string target;

    struct Artifact {
        std::string matcher;
        std::string data;
    } artifact;

    struct Branches {
        float android_version;
        std::string branch;
    };
    std::vector<Branches> branches;
};
using ROMs = std::vector<ROM>;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ROM::Artifact, matcher, data);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ROM::Branches, android_version, branch);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ROM, name, link, target, artifact, branches);

// targets.json
struct Device {
    std::string codename;
    std::string name;
    std::string manufacturer;
};
using Targets = std::vector<Device>;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Device, codename, name, manufacturer);

// versions.json
struct Version {
    float version;
    std::string codename_short;
    std::string codename;
};
using Versions = std::vector<Version>;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Version, version, codename_short, codename);

struct Manifest {
    std::string name;
    std::string url;
    struct Branches {
        std::string name;
        std::string target_rom;
        float android_version;
        std::string device;
        bool use_regex = false;
    };
    std::vector<Branches> branches;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Manifest::Branches, name, target_rom,
                                   android_version, device, use_regex);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Manifest, name, url, branches);

// recoveries.json
struct RecoveryManifest {
    std::string name;
    struct CloneMapping {
        std::string repo;
        std::string branch;
        std::string path;
    };
    std::vector<CloneMapping> clone_mappings;
    float android_version;
    std::string target_recovery;
    std::string device;
    bool use_regex = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RecoveryManifest::CloneMapping, repo, branch,
                                   path);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RecoveryManifest, name, clone_mappings,
                                   android_version, target_recovery, device,
                                   use_regex);

std::vector<ConfigParser::LocalManifest::Ptr> parse_manifest(
    const Manifest& manifest,
    const std::unordered_map<std::string, ConfigParser::Device::Ptr>& deviceMap,
    const std::unordered_map<std::string, ConfigParser::ROMBranch::Ptr>&
        romBranchMap) {
    std::vector<ConfigParser::LocalManifest::Ptr> manifests;
    for (const auto& branch : manifest.branches) {
        auto romKey = ConfigParser::ROMBranch::makeKey(branch.target_rom,
                                                       branch.android_version);
        auto romIt = romBranchMap.find(romKey);
        if (romIt == romBranchMap.end()) {
            LOG(WARNING) << fmt::format(
                "ROM branch not found for manifest {}: "
                "ROM {} Android version {}",
                manifest.name, branch.target_rom, branch.android_version);
            continue;
        }
        auto localManifest = std::make_shared<ConfigParser::LocalManifest>();
        localManifest->name = manifest.name;
        localManifest->rom = romIt->second;
        for (const auto& device : deviceMap) {
            if (branch.use_regex) {
                try {
                    if (!std::regex_match(device.first,
                                          std::regex(branch.device))) {
                        continue;
                    }
                } catch (const std::regex_error& e) {
                    LOG(ERROR)
                        << fmt::format("Invalid regex '{}' in manifest {}: {}",
                                       branch.device, manifest.name, e.what());
                    continue;
                }
            } else {
                if (device.first != branch.device) {
                    continue;
                }
            }
            localManifest->devices.push_back(device.second);
        }
        manifests.push_back(localManifest);
    }
    return manifests;
}

std::vector<ConfigParser::LocalManifest::Ptr> parse_recoverymanifest(
    const RecoveryManifest& manifest,
    const std::unordered_map<std::string, ConfigParser::Device::Ptr>& deviceMap,
    const std::unordered_map<std::string, ConfigParser::ROMBranch::Ptr>&
        romBranchMap) {
    std::vector<ConfigParser::LocalManifest::Ptr> manifests;
    // I just need to let c++ side know about recovery ROMs
    for (const auto& device : deviceMap) {
        if (manifest.use_regex) {
            try {
                if (!std::regex_match(device.first,
                                      std::regex(manifest.device))) {
                    continue;
                }
            } catch (const std::regex_error& e) {
                LOG(ERROR) << fmt::format(
                    "Invalid regex '{}' in recovery manifest {}: {}",
                    manifest.device, manifest.name, e.what());
                continue;
            }
        } else {
            if (device.first != manifest.device) {
                continue;
            }
        }
        auto romKey = ConfigParser::ROMBranch::makeKey(
            manifest.target_recovery, manifest.android_version);
        auto romIt = romBranchMap.find(romKey);
        if (romIt == romBranchMap.end()) {
            LOG(WARNING) << fmt::format(
                "ROM branch not found for recovery manifest {}: "
                "ROM {} Android version {}",
                manifest.name, manifest.target_recovery,
                manifest.android_version);
            continue;
        }
        auto localManifest = std::make_shared<ConfigParser::LocalManifest>();
        localManifest->name = manifest.name;
        localManifest->rom = romIt->second;
        localManifest->devices.push_back(device.second);
        manifests.push_back(localManifest);
    }
    return manifests;
}

bool is_json_file(const std::filesystem::path& path) {
    std::error_code ec;
    return path.extension() == ".json" &&
           std::filesystem::is_regular_file(path, ec);
}
}  // namespace ParseFiles

bool ConfigParser::merge() {
    DLOG(INFO) << "Merging configuration files from " << _jsonFileDir.string();
    DLOG(INFO) << "Loading targets.json";
    std::ifstream targets_jsonFile(_jsonFileDir / "targets.json");
    ParseFiles::Targets devices = nlohmann::json::parse(targets_jsonFile);
    DLOG(INFO) << "Loading versions.json";
    std::ifstream versions_jsonFile(_jsonFileDir / "versions.json");
    ParseFiles::Versions versions = nlohmann::json::parse(versions_jsonFile);
    DLOG(INFO) << "Loading roms.json";
    std::ifstream roms_jsonFile(_jsonFileDir / "roms.json");
    ParseFiles::ROMs roms = nlohmann::json::parse(roms_jsonFile);
    DLOG(INFO) << "Loading recoveries.json";
    std::ifstream recoveries_jsonFile(_jsonFileDir / "recoveries.json");
    ParseFiles::ROMs recoveries = nlohmann::json::parse(recoveries_jsonFile);
    DLOG(INFO) << "Building device and ROM maps";

    for (const auto& device : devices) {
        deviceMap[device.codename] =
            std::make_shared<Device>(device.codename, device.name);
    }
    for (const auto& version : versions) {
        auto androidVersion = std::make_shared<AndroidVersion>();
        androidVersion->version = version.version;
        androidVersion->name =
            fmt::format("{} ({},{})", version.codename, version.codename_short,
                        version.version);
        androidVersionMap[androidVersion->version] = androidVersion;
    }

    for (const auto& rom : roms) {
        auto romInfo = std::make_shared<ROMInfo>();
        romInfo->name = rom.name;
        romInfo->url = rom.link;
        romInfo->target = rom.target;
        romInfo->artifact.data = rom.artifact.data;
        romInfo->artifact.matcher = rom.artifact.matcher;

        for (const auto& branch : rom.branches) {
            auto romBranch = std::make_shared<ROMBranch>();
            romBranch->branch = branch.branch;
            romBranch->androidVersion =
                androidVersionMap[branch.android_version];
            romBranch->romInfo = romInfo;
            romBranchMap[romBranch->makeKey()] = romBranch;
        }
    }
    for (const auto& recovery : recoveries) {
        auto romInfo = std::make_shared<ROMInfo>();
        romInfo->name = recovery.name;
        romInfo->url = recovery.name;
        romInfo->target = recovery.target;
        romInfo->artifact.data = recovery.artifact.data;
        romInfo->artifact.matcher = recovery.artifact.matcher;

        for (const auto& branch : recovery.branches) {
            auto romBranch = std::make_shared<ROMBranch>();
            romBranch->branch = branch.branch;
            romBranch->androidVersion =
                androidVersionMap[branch.android_version];
            romBranch->romInfo = romInfo;
            romBranchMap[romBranch->makeKey()] = romBranch;
        }
    }

    DLOG(INFO) << "Scanning manifest files";
    std::error_code ec;
    // Scan through manifest/
    for (const auto& dirit :
         std::filesystem::directory_iterator(_jsonFileDir / "manifest", ec)) {
        if (ParseFiles::is_json_file(dirit.path())) {
            try {
                DLOG(INFO) << "Parsing manifest file: "
                           << dirit.path().filename();
                ParseFiles::Manifest manifest =
                    nlohmann::json::parse(std::ifstream(dirit.path()));
                DLOG(INFO) << "Manifest found: " << manifest.name;
                auto manifests = ParseFiles::parse_manifest(manifest, deviceMap,
                                                            romBranchMap);

                parsedManifests.insert(parsedManifests.end(), manifests.begin(),
                                       manifests.end());
                DLOG(INFO) << "Add file: " << dirit.path().filename() << ": "
                           << manifests.size();
            } catch (const nlohmann::json::parse_error& e) {
                LOG(ERROR) << "Failed to parse manifest file "
                           << dirit.path().filename() << ": " << e.what();
            }
        }
    }
    if (ec) {
        LOG(ERROR) << "Error scanning manifest directory: " << ec.message();
        return false;
    }

    // Scan through manifest/recovery
    DLOG(INFO) << "Scanning recovery manifest files";
    for (const auto& dirit : std::filesystem::directory_iterator(
             _jsonFileDir / "manifest" / "recovery", ec)) {
        if (ParseFiles::is_json_file(dirit.path())) {
            try {
                DLOG(INFO) << "Parsing recovery manifest file: "
                           << dirit.path().filename();
                ParseFiles::RecoveryManifest manifest =
                    nlohmann::json::parse(std::ifstream(dirit.path()));
                auto manifests = ParseFiles::parse_recoverymanifest(
                    manifest, deviceMap, romBranchMap);
                parsedManifests.insert(parsedManifests.end(), manifests.begin(),
                                       manifests.end());
                DLOG(INFO) << "Recovery Add file: " << dirit.path().filename()
                           << ": " << manifests.size();
            } catch (const nlohmann::json::parse_error& e) {
                LOG(ERROR) << "Failed to parse recovery manifest file "
                           << dirit.path().filename() << ": " << e.what();
            }
        }
    }
    if (ec) {
        LOG(ERROR) << "Error scanning recovery directory: " << ec.message();
        return false;
    }
    DLOG(INFO) << "Finished merging configuration files";

    // Now, clean the unreferenced device/roms
    std::erase_if(deviceMap, [](const auto& pair) {
        return pair.second.use_count() == 1;
    });
    std::erase_if(romBranchMap, [](const auto& pair) {
        return pair.second.use_count() == 1;
    });

    // Return the localManifests vector
    return !parsedManifests.empty();
}

ConfigParser::ConfigParser(std::filesystem::path jsonFileDir)
    : _jsonFileDir(std::move(jsonFileDir)) {
    // Load files
    LOG(INFO) << "Loading JSON files from directory: " << _jsonFileDir;
    merge();
}

std::vector<ConfigParser::LocalManifest::Ptr> ConfigParser::manifests() const {
    return parsedManifests;
}
