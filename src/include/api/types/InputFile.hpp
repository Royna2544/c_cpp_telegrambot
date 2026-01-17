#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace api::types {

/**
 * @brief This object represents the contents of a file to be uploaded.
 */
struct InputFile {
    /**
     * @brief Contents of a file.
     */
    std::string data;

    /**
     * @brief Mime type of a file.
     */
    std::string mimeType;

    /**
     * @brief File name.
     */
    std::string fileName;

    /**
     * @brief Creates new InputFile from an existing file.
     * 
     * @throws std::ios_base::failure if the file cannot be opened.
     */
    InputFile(const std::filesystem::path& filePath, std::string mimeType) : mimeType(mimeType), fileName(filePath.filename().string()) {
        std::ifstream file;
        file.exceptions(std::ios::badbit | std::ios::failbit);
        file.open(filePath, std::ios::binary);
        data = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    InputFile() = default;
};

}  // namespace api::types