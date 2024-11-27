#include "archives/Tar.hpp"

#include <absl/log/log.h>
#include <archive.h>
#include <archive_entry.h>
#include <fmt/format.h>

#include <filesystem>
#include <unordered_set>

#include "command_modules/support/CwdRestorar.hpp"
#include "trivial_helpers/raii.hpp"

Tar::Tar(const std::filesystem::path& file) { open(file); }

bool Tar::open(const std::filesystem::path& file) {
    _archive =
        RAII<archive*>::create<int>(archive_read_new(), &archive_read_close);

    // Enable all decompression filters and tar format
    archive_read_support_format_tar(_archive.get());
    archive_read_support_filter_all(
        _archive.get());  // Enables gzip, bzip2, xz, etc.

    // Open the archive file
    if (archive_read_open_filename(_archive.get(), file.string().c_str(),
                                   blocksize) != ARCHIVE_OK) {
        LOG(ERROR) << "Failed to open archive: "
                   << archive_error_string(_archive.get());
        return false;
    }
    DLOG(INFO) << "Opened archive: " << file;
    return true;
}

bool Tar::extract(const std::filesystem::path& path) const {
    struct archive* ext = archive_write_disk_new();
    struct archive_entry* entry = nullptr;
    bool ret = true;
    bool at_least_once_file = false;

    // Set extraction options
    int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
                ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS;

    archive_write_disk_set_options(ext, flags);

    // Store hard links for later processing
    std::vector<archive_entry*> hardLinks;

    // Iterate through each entry in the archive
    while (archive_read_next_header(_archive.get(), &entry) == ARCHIVE_OK) {
        at_least_once_file = true;

        // Get the full output path
        const char* currentFile = archive_entry_pathname(entry);
        std::filesystem::path fullPath = path / currentFile;

        // Check if the entry is a hard link
        if (archive_entry_hardlink(entry) != nullptr) {
            // Store hard links for the second pass
            hardLinks.emplace_back(archive_entry_clone(entry));
            continue;
        }

        // Set the path for extraction
        archive_entry_set_pathname(entry, fullPath.string().c_str());

        // Extract the entry
        if (archive_write_header(ext, entry) != ARCHIVE_OK) {
            LOG(ERROR) << "Failed to write header for " << currentFile << ": "
                       << archive_error_string(ext);
            ret = false;
            continue;
        }

        // Write the file contents
        const void* buffer{};
        size_t size{};
        la_int64_t offset{};
        while (archive_read_data_block(_archive.get(), &buffer, &size,
                                       &offset) == ARCHIVE_OK) {
            if (archive_write_data_block(ext, buffer, size, offset) !=
                ARCHIVE_OK) {
                LOG(ERROR) << "Failed to write data block: "
                           << archive_error_string(ext);
                ret = false;
                break;
            }
        }

        // Finish the entry
        archive_write_finish_entry(ext);
    }

    CwdRestorer cwd(path);

    if (!cwd) {
        LOG(ERROR) << "Error while pushing cwd";
        return false;
    }

    // Second pass: Extract hard links
    for (auto* hardLink : hardLinks) {
        const char* currentFile = archive_entry_pathname(hardLink);
        
        // Set the path for extraction
        archive_entry_set_pathname(hardLink, currentFile);

        if (archive_write_header(ext, hardLink) != ARCHIVE_OK) {
            LOG(ERROR) << "Failed to write header for hard link " << currentFile
                       << ": " << archive_error_string(ext);
            ret = false;
        }

        archive_write_finish_entry(ext);
        archive_entry_free(hardLink);
    }

    if (!at_least_once_file) {
        LOG(ERROR) << "Could not extract at least one file.";
        ret = false;
    }

    // Clean up
    archive_write_free(ext);
    return ret;
}

Tar::~Tar() = default;