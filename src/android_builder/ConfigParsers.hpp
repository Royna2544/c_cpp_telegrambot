#pragma once

#include <fstream>
#include <StringToolsExt.hpp>
#include <string>
#include <vector>
#include <absl/log/log.h>
#include "RepoUtils.hpp"

struct BuildConfig {
    static constexpr int elem_size = 6;
    static constexpr int indexOf_name = 0;
    static constexpr int indexOf_romName = 1;
    static constexpr int indexOf_localManifestUrl = 2;
    static constexpr int indexOf_localManifestBranch = 3;
    static constexpr int indexOf_device = 4;
    static constexpr int indexOf_variant = 5;

    std::string name;     // name of the build
    std::string romName;  // name of the ROM
    RepoUtils::CloneOptions local_manifest; // local manifest information
    std::string device;  // codename of the device
    enum class Variant {
        kUser,
        kUserDebug,
        kEng
    } variant;  // Target build variant
};

struct RomConfig {
    static constexpr int elem_size = 5;
    static constexpr int indexOf_name = 0;
    static constexpr int indexOf_url = 1;
    static constexpr int indexOf_branch = 2;
    static constexpr int indexOf_target = 3;
    static constexpr int indexOf_prefixOfOutput = 4;

    std::string name;     // name of the ROM
    std::string url;      // URL of the ROM repo
    std::string branch;   // branch of the repo
    std::string target;   // build target to build a ROM
    std::string prefixOfOutput; // prefix of output zip file
};

template <typename Data>
std::vector<Data> parse(std::ifstream data) = delete;

// Name RomName LocalManifestUrl LocalManifestBranch device variant 
template <>
std::vector<BuildConfig> parse(std::ifstream data);

// Name RepoUrl RepoBranch TargetName
template <>
std::vector<RomConfig> parse(std::ifstream data);
