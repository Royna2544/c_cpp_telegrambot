#include <absl/log/log.h>

#include <CommandLine.hpp>
#include <filesystem>
#include <stdexcept>
#include <system_error>

#include "Env.hpp"
#include "absl/strings/str_split.h"

CommandLine::CommandLine(CommandLine::argc_type argc,
                         CommandLine::argv_type argv)
    : _argc(argc), _argv(argv) {
    if (_argv == nullptr || _argv[0] == nullptr) {
        LOG(ERROR) << "Invalid argv passed";
        throw std::invalid_argument("Invalid argv passed");
    }
    exePath = std::filesystem::current_path() / std::filesystem::path(argv[0]);
    std::error_code ec;
    exePath = std::filesystem::canonical(exePath, ec);
    if (ec) {
        LOG(WARNING) << "Cannot fully resolve path";
#ifdef _WIN32
        constexpr const char kPathDelimiter = ';';
#else
        constexpr const char kPathDelimiter = ':';
#endif
        for (const auto& path : absl::StrSplit(Env()["PATH"].get(), kPathDelimiter)) {
            if (std::filesystem::is_regular_file(std::filesystem::path(path) /
                                                 argv[0])) {
                LOG(INFO) << "Found exepath";
                exePath = std::filesystem::path(path) / argv[0];
                break;
            }
        }
    }
    DLOG(INFO) << "exePath: " << exePath;
}

CommandLine::argv_type CommandLine::argv() const { return _argv; }

CommandLine::argc_type CommandLine::argc() const { return _argc; }

std::filesystem::path CommandLine::exe() const { return exePath; }

bool CommandLine::operator==(const CommandLine& other) const {
    if (_argc != other._argc) {
        return false;
    }
    for (int i = 0; i < _argc; ++i) {
        if (std::string_view(_argv[i]) != std::string_view(other._argv[i])) {
            return false;
        }
    }
    return true;
}

std::filesystem::path CommandLine::getPath(FS::PathType type) const {
    std::filesystem::path buf = exePath.parent_path();
    switch (type) {
        case FS::PathType::INSTALL_ROOT:
            return buf.parent_path();
        case FS::PathType::RESOURCES:
            return buf.parent_path() / "share" / "TgBot++";
        case FS::PathType::RESOURCES_SQL:
            return getPath(FS::PathType::RESOURCES) / "sql";
        case FS::PathType::RESOURCES_WEBPAGE:
            return buf.parent_path() / "www";
        case FS::PathType::RESOURCES_SCRIPTS:
            return getPath(FS::PathType::RESOURCES) / "scripts";
        case FS::PathType::CMD_MODULES:
            return buf.parent_path() / "lib" / "modules";
        default:
            break;
    }
    LOG(ERROR) << "Invalid path type: " << static_cast<int>(type);
    return {};
}