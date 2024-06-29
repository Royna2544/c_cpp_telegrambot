#pragma once

#include <filesystem>

/**
 * @brief Base class for photo manipulation.
 *
 * This class provides a base for photo manipulation operations such as reading,
 * writing, and transforming images.
 */
struct PhotoBase {
    /**
     * @brief Reads an image from the specified file.
     *
     * @param[in] filename The path to the image file.
     * @return True if the image was successfully read, false otherwise.
     */
    virtual bool read(const std::filesystem::path& filename) = 0;

    /**
     * @brief Rotates the image 90 degrees clockwise.
     */
    virtual void rotate_image_90() = 0;

    /**
     * @brief Rotates the image 180 degrees.
     */
    virtual void rotate_image_180() = 0;

    /**
     * @brief Rotates the image 270 degrees clockwise.
     */
    virtual void rotate_image_270() = 0;

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
};