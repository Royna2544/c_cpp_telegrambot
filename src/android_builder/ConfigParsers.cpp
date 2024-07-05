#include "ConfigParsers.hpp"

#include <absl/log/log.h>

constexpr bool parserDebug = true;

// Name RomName LocalManifestUrl LocalManifestBranch device variant
template <>
std::vector<BuildConfig> parse(std::ifstream data) {
    std::vector<BuildConfig> configs;
    std::string line;
    while (std::getline(data, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        BuildConfig config;
        std::vector<std::string> items;
        splitWithSpaces(line, items);
        if (items.size() != BuildConfig::elem_size) {
            LOG(WARNING) << "Skipping invalid line: " << line;
            continue;
        }
        config.name = items[BuildConfig::indexOf_name];
        config.romName = items[BuildConfig::indexOf_romName];
        config.local_manifest.url = items[BuildConfig::indexOf_localManifestUrl];
        config.local_manifest.branch = items[BuildConfig::indexOf_localManifestBranch];
        config.device = items[BuildConfig::indexOf_device];
        if (items[BuildConfig::indexOf_variant] == "user") {
            config.variant = BuildConfig::Variant::kUser;
        } else if (items[BuildConfig::indexOf_variant] == "userdebug") {
            config.variant = BuildConfig::Variant::kUserDebug;
        } else if (items[BuildConfig::indexOf_variant] == "eng") {
            config.variant = BuildConfig::Variant::kEng;
        } else {
            LOG(WARNING) << "Skipping invalid variant: " << items[BuildConfig::indexOf_variant];
            continue;
        }
        if (parserDebug) {
            LOG(INFO) << "---------------------------";
            LOG(INFO) << "Parsed config";
            LOG(INFO) << "Name: " << config.name;
            LOG(INFO) << "RomName: " << config.romName;
            LOG(INFO) << "LocalManifestUrl: " << config.local_manifest.url;
            LOG(INFO) << "LocalManifestBranch: "
                      << config.local_manifest.branch;
            LOG(INFO) << "Device: " << config.device;
            switch (config.variant) {
                case BuildConfig::Variant::kUser:
                    LOG(INFO) << "Variant: User";
                    break;
                case BuildConfig::Variant::kUserDebug:
                    LOG(INFO) << "Variant: UserDebug";
                    break;
                case BuildConfig::Variant::kEng:
                    LOG(INFO) << "Variant: Eng";
                    break;
            }
            LOG(INFO) << "---------------------------";
        }
        configs.emplace_back(config);
    }
    return configs;
}

template <>
std::vector<RomConfig> parse(std::ifstream data) {
    std::vector<RomConfig> roms;
    std::string line;
    while (std::getline(data, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        RomConfig rom;
        std::vector<std::string> items;
        splitWithSpaces(line, items);
        if (items.size() != RomConfig::elem_size) {
            LOG(WARNING) << "Skipping invalid line: " << line;
            if (parserDebug) {
                LOG(INFO) << "Invalid rom config size: " << items.size();
                int count = 0;
                for (const auto& item : items) {
                    LOG(INFO) << '[' << count << "]=" << item;
                }
            }
            continue;
        }
        rom.name = items[RomConfig::indexOf_name];
        rom.url = items[RomConfig::indexOf_url];
        rom.branch = items[RomConfig::indexOf_branch];
        rom.target = items[RomConfig::indexOf_target];
        rom.prefixOfOutput = items[RomConfig::indexOf_prefixOfOutput];
        if (parserDebug) {
            LOG(INFO) << "---------------------------";
            LOG(INFO) << "Parsed rom";
            LOG(INFO) << "Name: " << rom.name;
            LOG(INFO) << "Url: " << rom.url;
            LOG(INFO) << "Branch: " << rom.branch;
            LOG(INFO) << "Target: " << rom.target;
            LOG(INFO) << "PrefixOfOutput: " << rom.prefixOfOutput;
            LOG(INFO) << "---------------------------";
        }
        roms.emplace_back(rom);
    }
    return roms;
}