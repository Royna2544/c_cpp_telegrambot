#pragma once

#include <absl/status/status.h>
#include <cstddef>
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
    absl::Status read(const std::filesystem::path& filename, const Target target) override;

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
    absl::Status processAndWrite(const std::filesystem::path& filename) override;

    std::string version() const override;

   private:
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
    absl::Status rotate(int angle);

    /**
     * @brief Converts the image to grayscale.
     *
     * This function converts the image to grayscale by averaging the RGB values
     * of each pixel. The internal data of the image is updated accordingly.
     */
    void greyscale();

    void invert();

    using webpimage_size_t = int;
    webpimage_size_t width_{};
    webpimage_size_t height_{};
    std::unique_ptr<uint8_t[]> data_;
    constexpr static float QUALITY = .5f;
};
