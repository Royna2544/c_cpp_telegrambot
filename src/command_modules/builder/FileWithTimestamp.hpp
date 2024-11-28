#pragma once

#include <chrono>
#include <filesystem>
#include <system_error>
#include <utility>

struct FileWithTimestamp {
    std::filesystem::path path;
    std::filesystem::file_time_type lastModified;

    explicit FileWithTimestamp(std::filesystem::path _path)
        : path(std::move(_path)) {
        std::error_code ec;
        lastModified = std::filesystem::last_write_time(path, ec);
        if (ec) {
            throw std::system_error(ec, "Error getting file modification time");
        }
    }

    [[nodiscard]] bool updated() {
        std::error_code ec;
        auto tmp = std::filesystem::last_write_time(path, ec);
        if (ec) {
            throw std::system_error(ec, "Error getting file modification time");
        }
        if (tmp != lastModified) {
            lastModified = tmp;
            return true;
        }
        return false;
    }
};