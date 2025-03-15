#pragma once

#include <archive.h>
#include <archive_entry.h>

#include <filesystem>
#include <vector>

class Zip {
    struct archive* archive = nullptr;

    static bool acceptFile(const std::filesystem::directory_entry& entry);
    std::vector<std::string> filesAdded;

   public:
    Zip(const Zip&) = delete;
    Zip& operator=(const Zip&) = delete;

    ~Zip() {
        if (archive != nullptr) {
            save();
        }
    }
    explicit Zip(const std::filesystem::path& path) { open(path); }
    Zip() = default;

    /**
     * @brief Opens a zip file for writing.
     *
     * This function opens a zip file at the specified path for writing. If the
     * file already exists, it will be overwritten.
     *
     * @param path The path to the zip file to open.
     * @return true if the zip file was successfully opened for writing, false
     * otherwise.
     */
    bool open(const std::filesystem::path& path);

    /**
     * @brief Adds a file to the zip archive.
     *
     * This function adds a file from the specified filepath to the zip archive
     * with the given entryname.
     *
     * @param filepath The path to the file to add to the zip archive.
     * @param entryname The name to use for the file in the zip archive.
     * @return true if the file was successfully added to the zip archive, false
     * otherwise.
     */
    bool addFile(const std::filesystem::path& filepath,
                 const std::string_view entryname);

    /**
     * @brief Adds a directory to the zip archive.
     *
     * This function adds a directory from the specified dirpath to the zip
     * archive. The directory will be added recursively, and the ziproot
     * parameter specifies the root directory within the zip archive.
     *
     * @param dirpath The path to the directory to add to the zip archive.
     * @param ziproot The root directory within the zip archive where the
     * directory will be placed.
     * @return true if the directory was successfully added to the zip archive,
     * false otherwise.
     */
    bool addDir(const std::filesystem::path& dirpath,
                const std::filesystem::path& ziproot);

    /**
     * @brief Saves the zip archive.
     *
     * This function saves the zip archive to disk. It must be called after
     * adding files and directories to the archive.
     *
     * @return true if the zip archive was successfully saved, false otherwise.
     */
    bool save();

    static bool extract(const std::filesystem::path& zipfile,
                        const std::filesystem::path& output_dir);
};
