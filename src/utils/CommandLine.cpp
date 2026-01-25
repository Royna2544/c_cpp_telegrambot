#include <absl/log/log.h>

#include <CommandLine.hpp>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <trivial_helpers/log_once.hpp>

#include "Env.hpp"

CommandLine::CommandLine(CommandLine::argc_type argc,
                         CommandLine::argv_type argv)
    : _argc(argc), _argv(argv) {
    std::error_code ec;
    if (_argv == nullptr || _argv[0] == nullptr) {
        LOG(ERROR) << "Invalid argv passed";
        throw std::invalid_argument("Invalid argv passed");
    }

    LOG_ONCE(INFO) << "Try autodetect exePath";
    const auto p1 = std::filesystem::current_path() / argv[0];
    exePath = std::filesystem::canonical(p1, ec);
    if (ec) {
        LOG_ONCE(WARNING) << "Try 1: " << p1 << ": " << ec.message();
        const auto p2 = std::filesystem::path(INSTALL_PREFIX) / argv[0];
        exePath = std::filesystem::canonical(p2, ec);
        if (ec) {
            LOG_ONCE(WARNING) << "Try 2: " << p2 << ": " << ec.message();
            if (Env()["GLIDER_ROOT"].has()) {
                const auto p3 =
                    std::filesystem::path(Env()["GLIDER_ROOT"].get()) / argv[0];
                exePath = std::filesystem::canonical(p3, ec);
                if (ec) {
                    LOG_ONCE(ERROR) << "Try 3: " << p3 << ": " << ec.message();
                    LOG_ONCE(ERROR) << "Failed...";
                } else {
                    LOG_ONCE(INFO) << exePath << ": OK";
                }
            }
        } else {
            LOG_ONCE(INFO) << exePath << ": OK";
        }
    } else {
        LOG_ONCE(INFO) << exePath << ": OK";
    }
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
            return buf.parent_path() / "share" / "Glider";
        case FS::PathType::RESOURCES_SQL:
            return getPath(FS::PathType::RESOURCES) / "sql";
        case FS::PathType::RESOURCES_WEBPAGE:
            return getPath(FS::PathType::RESOURCES) / "www";
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
