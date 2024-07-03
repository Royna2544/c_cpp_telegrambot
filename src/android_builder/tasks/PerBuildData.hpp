#pragma once

#include <ConfigParsers.hpp>

struct PerBuildData {
    BuildConfig bConfig;
    RomConfig rConfig;
    std::filesystem::path scriptDirectory;
    bool* result;
};
