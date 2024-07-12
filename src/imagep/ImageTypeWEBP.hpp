#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

#include "ImagePBase.hpp"

class WebPImage : public PhotoBase {
   public:
    WebPImage() noexcept = default;
    ~WebPImage() override = default;

    /**
     * @brief Reads an image from the specified file path.
     *
     * This function reads an image from the specified file path and stores it
     * in the object's internal data.
     *
     * @param filename The path to the image file.
     *
     * @return True if the image is successfully read, false otherwise.
     */
    bool read(const std::filesystem::path& filename) override;

    /**
     * @brief Rotates the image by the specified angle in degrees.
     *
     * This method rotates the image by the specified angle in degrees. The
     * internal data of the image is updated accordingly.
     *
     * @param angle The angle in degrees by which the image should be rotated.
     *
     * @return A Result object indicating the success or failure of the
     * operation.
     */
    absl::Status _rotate_image(int angle) override;

    /**
     * @brief Converts the image to grayscale.
     *
     * This function converts the image to grayscale by averaging the RGB values
     * of each pixel. The internal data of the image is updated accordingly.
     */
    void to_greyscale() override;

    /**
     * @brief Writes the image to the specified file path.
     *
     * This function writes the image to the specified file path. The internal
     * data of the image is saved to the file.
     *
     * @param filename The path to the image file.
     *
     * @return True if the image is successfully written, false otherwise.
     */
    bool write(const std::filesystem::path& filename) override;

    std::string version() const override;

   private:
    long width_{};
    long height_{};
    std::unique_ptr<uint8_t[]> data_;
};
