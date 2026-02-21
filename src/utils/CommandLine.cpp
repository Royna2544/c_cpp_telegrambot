#include <absl/log/log.h>

#include <CommandLine.hpp>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <trivial_helpers/log_once.hpp>

#include "Env.hpp"

#ifdef __linux__
#include <unistd.h>

#include <climits>

namespace {
std::filesystem::path getExecutablePath() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return std::string(result, (count > 0) ? count : 0);
}
}  // namespace
#elif defined(_WIN32)
#include <windows.h>

namespace {
std::filesystem::path getExecutablePath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::filesystem::path(path);
}
}  // namespace
#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>

namespace {
std::filesystem::path getExecutablePath() {
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::filesystem::path(path);
    }
    return {};
}
}  // namespace
#else
#error "Unsupported platform"
#endif

CommandLine::CommandLine(CommandLine::argc_type argc,
                         CommandLine::argv_type argv)
    : _argc(argc), _argv(argv) {
    std::error_code ec;
    if (_argv == nullptr || _argv[0] == nullptr) {
        LOG(ERROR) << "Invalid argv passed";
        throw std::invalid_argument("Invalid argv passed");
    }

    exePath = std::filesystem::canonical(getExecutablePath(), ec);
    if (ec) {
        LOG(ERROR) << "Failed to get executable path: " << ec.message();
        throw std::runtime_error("Failed to get executable path");
    }
    DLOG(INFO) << "Executable path: " << exePath;
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
