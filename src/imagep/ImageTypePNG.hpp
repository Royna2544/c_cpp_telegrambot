#pragma once

#include <png.h>
#include <pngconf.h>

#include <SharedMalloc.hpp>
#include <cstddef>
#include <filesystem>
#include <vector>

#include "ImagePBase.hpp"

struct PngImage : PhotoBase {
    PngImage() noexcept = default;
    ~PngImage() override = default;

    struct PngRefMem {
        void add(std::size_t size) {
            auto m = SharedMalloc(size);
            row_data.emplace_back(static_cast<png_bytep>(m.get()));
            mallocs.emplace_back(std::move(m));
        }
        void resize(std::size_t size) { row_data.resize(size); }
        [[nodiscard]] png_bytepp data() { return row_data.data(); }
        png_bytep& operator[](std::size_t size) { return row_data[size]; }

       private:
        std::vector<SharedMalloc> mallocs;
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
    TinyStatus read(const std::filesystem::path& filename,
                    const Target target) override;

    /**
     * @brief Writes the image to the specified file path.
     *
     * @param filename The path to write the image to.
     *
     * @return A Result object that indicates whether the writing operation was
     * successful.
     */
    TinyStatus processAndWrite(const std::filesystem::path& filename) override;

    [[nodiscard]] std::string version() const override;

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
    void rotate_image_impl(png_uint_32 new_width, png_uint_32 new_height,
                           const transform_fn_t& transform);

    void greyscale();
    void invert();
    TinyStatus rotate(int angle);
};