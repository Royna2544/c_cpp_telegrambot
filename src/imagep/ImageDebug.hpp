#pragma once

#include "ImagePBase.hpp"

struct DebugImage : public PhotoBase {
    /**
     * @brief Reads an image from the specified file.
     *
     * @param[in] filename The path to the image file.
     * @param[in] target Target specification for the image reading process.
     * @return The result of the read operation
     */
    TinyStatus read(const std::filesystem::path& filename,
                Target target = Target::kNone) override;

   
    /**
     * @brief Processes and writes the image to the specified file.
     *
     * This function applies the specified options (if any) to the image and
     * writes the processed image to the specified file. The options include
     * rotation, greyscale conversion, color inversion, and destination file
     * path.
     *
     * @param[in] filename The path to the output image file.
     *
     * @return A TinyStatus indicating the success or failure of the
     * operation.
     * - Status::kOk: The operation was successful.
     * - Status::kWriteError: Failed to write the image to the specified file.
     * - Status::kReadError: Failed to read the image from the source file.
     * - Status::kInvalidArgument: Invalid input parameters.
     * - Status::kProcessingError: The image is not valid or cannot
     * be processed.
     * - Status::kUnknown: An unknown error occurred during the
     * operation.
     */
    TinyStatus processAndWrite(const std::filesystem::path& filename) override;

    /**
     * @brief Destructor for the photo manipulation base class.
     */
    ~DebugImage() override = default;

    [[nodiscard]] std::string version() const override {
        return "DebugImage v1.0.0";
    }
};
