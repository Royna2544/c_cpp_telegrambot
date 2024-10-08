#pragma once

#include <absl/log/log.h>
#include <absl/status/status.h>

#include <filesystem>

/**
 * @brief Base class for photo manipulation.
 *
 * This class provides a base for photo manipulation operations such as reading,
 * writing, and transforming images.
 */
struct PhotoBase {
    static constexpr int kAngleMin = 0;
    static constexpr int kAngle90 = 90;
    static constexpr int kAngle180 = 180;
    static constexpr int kAngle270 = 270;
    static constexpr int kAngleMax = 360;

    /**
     * @brief Reads an image from the specified file.
     *
     * @param[in] filename The path to the image file.
     * @return True if the image was successfully read, false otherwise.
     */
    virtual bool read(const std::filesystem::path& filename) = 0;

    /**
     * @brief Rotates the image by the specified angle.
     *
     * This function rotates the image by the given angle in degrees. The
     * rotation is performed counter-clockwise.
     *
     * @param[in] angle The angle in degrees to rotate the image. The valid
     * range is from 0 to 360.
     *
     * @note The function does not handle cases where the image is not valid or
     *       cannot be rotated.
     */
    absl::Status rotate_image(int angle) {
        if (angle < kAngleMin || angle > kAngleMax) {
            LOG(ERROR) << "Invalid rotation angle: " << angle;
            return absl::InvalidArgumentError("Invalid rotation angle");
        }
        return _rotate_image(angle);
    }

    /**
     * @brief Converts the image to grayscale.
     */
    virtual void to_greyscale() = 0;

    /**
     * @brief Writes the image to the specified file.
     *
     * @param[in] filename The path to the image file.
     * @return True if the image was successfully written, false otherwise.
     */
    virtual bool write(const std::filesystem::path& filename) = 0;

    /**
     * @brief Destructor for the photo manipulation base class.
     */
    virtual ~PhotoBase() = default;

    [[nodiscard]] virtual std::string version() const = 0;

   protected:
    /**
     * @brief Rotates the image by the specified angle.
     *
     * This function rotates the image by the given angle in degrees. The
     * rotation is performed counter-clockwise.
     *
     * @param[in] angle The angle in degrees to rotate the image. The valid
     * range is from 0 to 360.
     *
     * @note The function does not handle cases where the image is not valid or
     *       cannot be rotated.
     */
    virtual absl::Status _rotate_image(int angle) = 0;
};