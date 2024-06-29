#include <absl/log/log.h>
#include <png.h>
#include <pngconf.h>

#include <filesystem>
#include <libPHOTOBase.hpp>
#include <vector>

struct PngImage : PhotoBase {
    PngImage() = default;
    ~PngImage() override = default;

    png_uint_32 width{}, height{};
    png_byte color_type{}, bit_depth{};
    std::vector<png_bytep> row_data;
    std::vector<size_t> row_size;
    bool contains_data = false;
    /**
     * @brief Creates a unique_ptr that will automatically close the given FILE*
     * when it goes out of scope.
     *
     * @param file The FILE* to be closed when the unique_ptr goes out of scope.
     *
     * @return A unique_ptr that will automatically close the given FILE* when
     * it goes out of scope.
     */
    static auto createFileCloser(FILE* file) {
        return std::unique_ptr<FILE, void (*)(FILE*)>(
            file, [](FILE* f) { fclose(f); });
    }

    /**
     * @brief Reads a PNG image from the specified file path.
     *
     * @param filename The path to the PNG image file.
     *
     * @return True if the image was successfully read, false otherwise.
     */
    bool read(const std::filesystem::path& filename) override;

    /**
     * @brief Rotates the image 90 degrees clockwise.
     */
    void rotate_image_90() override;

    /**
     * @brief Rotates the image 180 degrees.
     */
    void rotate_image_180() override;

    /**
     * @brief Rotates the image 270 degrees clockwise.
     */
    void rotate_image_270() override;

    /**
     * @brief Converts the image to grayscale.
     */
    void to_greyscale() override;

    /**
     * @brief Writes the image to the specified file path.
     *
     * @param filename The path to write the image to.
     *
     * @return True if the image was successfully written, false otherwise.
     */
    bool write(const std::filesystem::path& filename) override;

   private:
    using transform_fn_t = std::function<void(
        int src_width, int src_height, int& dst_x, int& dst_y, int x, int y)>;

    /**
     * @brief Rotates the image with the specified new dimensions and
     * transformation function.
     *
     * @param new_width The new width of the image.
     * @param new_height The new height of the image.
     * @param transform A function that takes the source coordinates and
     * transforms them to the destination coordinates.
     */
    void rotate_image(int new_width, int new_height, transform_fn_t transform);
};