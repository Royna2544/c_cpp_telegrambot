#pragma once

#include <filesystem>
#include <AbslLogCompat.hpp>
#include "utils/libfs.hpp"

class CwdRestorer {
    std::filesystem::path cwd;
    std::error_code ec;

   public:
    CwdRestorer(const CwdRestorer&) = delete;
    CwdRestorer& operator=(const CwdRestorer&) = delete;

    explicit CwdRestorer(const std::filesystem::path& newCwd) {
        cwd = std::filesystem::current_path(ec);
        if (ec) {
            LOG(ERROR) << "Failed to get current cwd: " << ec.message();
            return;
        }

        LOG(INFO) << "Changing cwd to: " << newCwd.string();
        std::filesystem::current_path(newCwd, ec);
        if (!ec) {
            // Successfully changed cwd
            return;
        }

        // If we didn't get ENOENT, then nothing we can do here
        if (ec == std::errc::no_such_file_or_directory) {
            // If it was no such file or directory, then we could try to create
            if (!noex_fs::create_directories(newCwd)) {
                LOG(ERROR) << "Failed to create build directory: "
                           << ec.message();
            }
            // Now try to change cwd again
            std::filesystem::current_path(newCwd, ec);
            LOG(INFO) << "Successfully changed cwd with creation";
        } else {
            LOG(ERROR) << "Unexpected error while changing cwd: "
                       << ec.message();
        }
    }

    ~CwdRestorer() {
        LOG(INFO) << "Restoring cwd to: " << cwd;
        std::filesystem::current_path(cwd, ec);
        if (ec) {
            LOG(ERROR) << "Error while restoring cwd: " << ec.message();
        }
    }

    operator bool() const noexcept { return !ec; }
};
