#include "Zip.hpp"

#include <absl/log/log.h>
#include <fmt/core.h>

#include <fstream>
#include <iostream>

bool Zip::open(const std::filesystem::path& path) {
    // Create a new zip archive for writing
    archive = archive_write_new();
    if (archive == nullptr) {
        LOG(ERROR) << "Failed to create libarchive object.";
        return false;
    }

    // Set the archive format to ZIP
    if (archive_write_set_format_zip(archive) != ARCHIVE_OK) {
        LOG(ERROR) << "Failed to set archive format: "
                   << archive_error_string(archive);
        return false;
    }

    // Open the file for writing
    if (archive_write_open_filename(archive, path.string().c_str()) !=
        ARCHIVE_OK) {
        LOG(ERROR) << "Failed to open output file: "
                   << archive_error_string(archive);
        return false;
    }

    return true;
}

bool Zip::addFile(const std::filesystem::path& filepath,
                  const std::string_view entryname) {
    // Open the file for reading
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        LOG(ERROR) << "Cannot open file: " << filepath;
        return false;
    }

    // Get file size
    std::error_code ec;
    uintmax_t filesize = std::filesystem::file_size(filepath, ec);
    if (ec) {
        LOG(ERROR) << "Failed to check file size: " << ec.message();
        return false;
    } else if (filesize == 0) {
        LOG(WARNING) << "Skipping empty file: " << filepath;
        return true;
    }

    // Create archive entry
    struct archive_entry* entry = archive_entry_new();
    archive_entry_set_pathname(entry, entryname.data());
    archive_entry_set_size(entry, filesize);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);

    // Write entry header
    if (archive_write_header(archive, entry) != ARCHIVE_OK) {
        LOG(ERROR) << "Failed to write entry header: "
                   << archive_error_string(archive);
        archive_entry_free(entry);
        return false;
    }

    // Write file contents
    constexpr int BUFSIZE = 8192;
    std::array<char, BUFSIZE> buffer{};
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        if (archive_write_data(archive, buffer.data(), file.gcount()) < 0) {
            LOG(ERROR) << "Failed to write file data: "
                       << archive_error_string(archive);
            archive_entry_free(entry);
            return false;
        }
    }

    archive_entry_free(entry);
    return true;
}

bool Zip::addDir(const std::filesystem::path& dirpath,
                 const std::filesystem::path& ziproot) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dirpath, ec)) {
        const auto zipentry = ziproot / entry.path().filename();
        if (entry.is_directory()) {
            addDir(entry.path(), zipentry);
        } else if (entry.is_regular_file() &&
                   !entry.path().filename().string().starts_with(".")) {
            addFile(entry.path(), zipentry.string());
        } else {
            LOG(ERROR) << "Skipping non-regular file: " << entry.path();
        }
    }
    if (ec) {
        LOG(ERROR) << "Error while iterating over directory: " << ec.message();
        return false;
    }
    return true;
}

bool Zip::save() {
    if (archive == nullptr) {
        LOG(ERROR) << "Cannot save zip archive, it's already closed";
        return false;
    }

    if (archive_write_close(archive) != ARCHIVE_OK) {
        LOG(ERROR) << "Failed to close zip archive: "
                   << archive_error_string(archive);
        return false;
    }
    archive_write_free(archive);
    archive = nullptr;
    return true;
}