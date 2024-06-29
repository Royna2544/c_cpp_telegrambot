#include <cstdint>
#include <filesystem>
#include <libPHOTOBase.hpp>
#include <memory>

class WebPImage : PhotoBase {
   public:
    WebPImage() = default;
    ~WebPImage() override = default;

    enum class Degrees {
        DEGREES_90,
        DEGREES_180,
        DEGREES_270
    };
    
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
     * @brief Rotates the image 90 degrees clockwise.
     *
     * This function rotates the image 90 degrees clockwise. The internal data
     * of the image is updated accordingly.
     */
    void rotate_image_90() override;

    /**
     * @brief Rotates the image 180 degrees.
     *
     * This function rotates the image 180 degrees. The internal data of the
     * image is updated accordingly.
     */
    void rotate_image_180() override;

    /**
     * @brief Rotates the image 270 degrees clockwise.
     *
     * This function rotates the image 270 degrees clockwise. The internal data
     * of the image is updated accordingly.
     */
    void rotate_image_270() override;

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

   private:
    long width_{};
    long height_{};
    std::unique_ptr<uint8_t[]> data_;

    void rotateImage(Degrees degrees);
};
