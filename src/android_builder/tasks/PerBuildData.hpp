#pragma once

#include <ConfigParsers.hpp>

struct PerBuildData {
    enum class Result { SUCCESS, ERROR_NONFATAL, ERROR_FATAL };
    BuildConfig bConfig;
    RomConfig rConfig;
    std::filesystem::path scriptDirectory;
    struct ResultData {
        Result value;
        std::string msg;
    } *result;
};
