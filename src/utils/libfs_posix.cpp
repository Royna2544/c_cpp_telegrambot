#include <absl/log/log.h>

#include <cstdlib>
#include <filesystem>

#include "Env.hpp"
#include "libfs.hpp"

bool FS::getHomePath(std::filesystem::path& buf) {
    return Env{}["HOME"].assign(buf);
}