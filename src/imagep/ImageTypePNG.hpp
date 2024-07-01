#pragma once

#include <png.h>
#include <pngconf.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

#include "ImagePBase.hpp"

struct PngImage : PhotoBase {
    PngImage() noexcept = default;
    ~PngImage() override = default;

    struct MiniSharedMalloc {
        MiniSharedMalloc(std::size_t size) : data(malloc(size)) {}
        MiniSharedMalloc() = default;
        ~MiniSharedMalloc() {
            if (data)  // Only free if data is not nullptr.
                free(data);
        }
        void* data = nullptr;
    };

    struct PngRefMem {
        void add(std::size_t size) {
            auto m = std::make_shared<MiniSharedMalloc>(size);
            mallocs.emplace_back(m);
            row_data.emplace_back(static_cast<png_bytep>(m->data));
        }
        void resize(std::size_t size) { row_data.resize(size); }
        [[nodiscard]] png_bytepp data() { return row_data.data(); }
        png_bytep& operator[](std::size_t size) { return row_data[size]; }

       private:
        std::vector<std::shared_ptr<MiniSharedMalloc>> mallocs;
        std::vector<png_bytep> row_data;
    } refmem;

    png_uint_32 width{}, height{};
    png_byte color_type{}, bit_depth{};
    bool contains_data = false;

    /**
     * @brief Reads a PNG image from the specified file path.
     *
     * @param filename The path to the PNG image file.
     *
     * @return True if the image was successfully read, false otherwise.
     */
    bool read(const std::filesystem::path& filename) override;

    /**
     * @brief Rotates the image with the specified new dimensions and
     * transformation function.
     *
     * @param angle The angle of rotation.
     *
     * @return A Result object that indicates whether the rotation operation was
     * successful.
     */
    Result _rotate_image(int angle) override;

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

    std::string version() const override;
    
   private:
    using transform_fn_t =
        std::function<void(png_uint_32 src_width, png_uint_32 src_height,
                           ptrdiff_t& dst_x, ptrdiff_t& dst_y, int x, int y)>;

    /**
     * @brief Rotates the image with the specified new dimensions and
     * transformation function.
     *
     * @param new_width The new width of the image.
     * @param new_height The new height of the image.
     * @param transform A function that takes the source coordinates and
     * transforms them to the destination coordinates.
     */
    void rotate_image_impl(int new_width, int new_height,
                           transform_fn_t transform);
};