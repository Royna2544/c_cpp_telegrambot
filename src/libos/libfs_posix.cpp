#include <cstdlib>
#include <filesystem>
#include <Logging.h>
#include "libfs.hpp"

bool FS::exists(const std::filesystem::path& filename) {
    bool rc;

    std::error_code ec;
    rc = std::filesystem::exists(filename, ec);
    if (ec) {
        LOG(LogLevel::WARNING, "FS::Exists failed: %s", ec.message().c_str());
    }
    return rc;
}

bool FS::getHomePath(std::filesystem::path& buf) {
    auto buf_c = getenv("HOME");
    if (buf_c) {
        buf = buf_c;
    }
    return !!buf_c;
}
