#include <absl/log/log.h>

#include <cstdlib>
#include <filesystem>

#include "libfs.hpp"

bool FS::exists(const std::filesystem::path& filename) {
    bool rc;

    std::error_code ec;
    rc = std::filesystem::exists(filename, ec);
    if (ec) {
        PLOG(WARNING) << "FS::Exists failed: " << ec.message();
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
