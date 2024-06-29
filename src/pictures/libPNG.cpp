#include "libPNG.hpp"

#include <absl/log/log.h>
#include <png.h>
#include <pngconf.h>

#include <cstddef>
#include <filesystem>
#include <vector>

bool PngImage::read(const std::filesystem::path& filename) {
    FILE* fp = nullptr;
    png_structp png = nullptr;
    png_infop info = nullptr;

    if (contains_data) {
        LOG(WARNING) << "Already contains data, ignore";
        return false;
    }

    LOG(INFO) << "Loading image " << filename;

    fp = fopen(filename.string().c_str(), "rb");
    if (fp == nullptr) {
        LOG(ERROR) << "Can't open file " << filename << " for reading";
        return false;
    }

    const auto fileCloser = createFileCloser(fp);

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                 nullptr);
    if (png == nullptr) {
        LOG(ERROR) << "png_create_read_struct failed";
        return false;
    }

    info = png_create_info_struct(png);
    if (info == nullptr) {
        LOG(ERROR) << "png_create_info_struct failed";
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        LOG(ERROR) << "Error during reading image header";
        png_destroy_read_struct(&png, &info, nullptr);
        return false;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    color_type = png_get_color_type(png, info);
    bit_depth = png_get_bit_depth(png, info);

    if (setjmp(png_jmpbuf(png))) {
        LOG(ERROR) << "Error during updating image information";
        png_destroy_read_struct(&png, &info, nullptr);
        return false;
    }
    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS) != 0U) {
        png_set_tRNS_to_alpha(png);
    }
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    png_read_update_info(png, info);

    row_data.resize(height);
    row_size.resize(height);
    for (int y = 0; y < height; y++) {
        const size_t byteSz = png_get_rowbytes(png, info);
        row_data[y] = static_cast<png_bytep>(malloc(byteSz));
        row_size[y] = byteSz;
    }

    png_read_image(png, row_data.data());

    png_destroy_read_struct(&png, &info, nullptr);

    LOG(INFO) << "Read image: " << height << "x" << width;
    contains_data = true;
    return true;
}

void PngImage::to_greyscale() {
    if (!contains_data) {
        LOG(ERROR) << "No image data to convert to greyscale";
        return;
    }
    LOG(INFO) << "Converting image to greyscale";
    for (int y = 0; y < height; y++) {
        png_bytep row = row_data[y];
        for (int x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]);
            uint8_t gray = static_cast<uint8_t>(0.299 * px[0] + 0.587 * px[1] +
                                                0.114 * px[2]);
            px[0] = px[1] = px[2] = gray;
        }
    }
}

void PngImage::rotate_image(int new_width, int new_height,
                            transform_fn_t transform) {
    PngImage src = *this;

    width = new_width;
    height = new_height;
    color_type = src.color_type;
    bit_depth = src.bit_depth;
    row_data.resize(height);
    row_size.resize(height);

    for (int y = 0; y < height; y++) {
        row_data[y] = (png_byte*)malloc(static_cast<size_t>(width) * 4);
    }

    for (int y = 0; y < src.height; y++) {
        for (int x = 0; x < src.width; x++) {
            int dst_x, dst_y;
            transform(src.width, src.height, dst_x, dst_y, x, y);
            png_bytep src_pixel = &src.row_data[y][x * 4];
            png_bytep dst_pixel = &row_data[dst_y][dst_x * 4];
            memcpy(dst_pixel, src_pixel, 4);
        }
    }

    for (auto* const row : src.row_data) {
        free(row);
    }
}

void PngImage::rotate_image_90() {
    if (!contains_data) {
        LOG(ERROR) << "No image data to rotate";
        return;
    }

    LOG(INFO) << "Rotating image by 90 degrees";
    rotate_image(height, width,
                 [](int src_width, int src_height, int& dst_x, int& dst_y,
                    int x, int y) {
                     dst_x = src_height - 1 - y;
                     dst_y = x;
                 });
    LOG(INFO) << "New dimensions: " << width << "x" << height;
}

void PngImage::rotate_image_180() {
    if (!contains_data) {
        LOG(ERROR) << "No image data to rotate";
        return;
    }
    LOG(INFO) << "Rotating image by 180 degrees";
    rotate_image(width, height,
                 [](int src_width, int src_height, int& dst_x, int& dst_y,
                    int x, int y) {
                     dst_x = src_width - 1 - x;
                     dst_y = src_height - 1 - y;
                 });
}

void PngImage::rotate_image_270() {
    if (!contains_data) {
        LOG(ERROR) << "No image data to rotate";
        return;
    }
    LOG(INFO) << "Rotating image by 270 degrees";
    rotate_image(height, width,
                 [](int src_width, int src_height, int& dst_x, int& dst_y,
                    int x, int y) {
                     dst_x = y;
                     dst_y = src_width - 1 - x;
                 });
    LOG(INFO) << "New dimensions: " << width << "x" << height;
}

bool PngImage::write(const std::filesystem::path& filename) {
    FILE* fp = nullptr;
    png_structp png = nullptr;
    png_infop info = nullptr;

    if (!contains_data) {
        LOG(ERROR) << "No image data to write";
        return false;
    }

    fp = fopen(filename.string().c_str(), "wb");
    if (fp == nullptr) {
        LOG(ERROR) << "Can't open file " << filename << " for writing";
        return false;
    }
    const auto fileCloser = createFileCloser(fp);

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                  nullptr);
    if (png == nullptr) {
        LOG(ERROR) << "png_create_write_struct failed";
        return false;
    }

    info = png_create_info_struct(png);
    if (info == nullptr) {
        LOG(ERROR) << "png_create_info_struct failed";
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        LOG(ERROR) << "Error during init_io";
        png_destroy_write_struct(&png, &info);
        return false;
    }

    png_init_io(png, fp);

    if (setjmp(png_jmpbuf(png))) {
        LOG(ERROR) << "Error during writing header";
        return false;
    }

    png_set_IHDR(png, info, width, height, bit_depth, color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    if (setjmp(png_jmpbuf(png))) {
        LOG(ERROR) << "Error during writing bytes";
        png_destroy_write_struct(&png, &info);
        return false;
    }

    png_write_image(png, row_data.data());

    if (setjmp(png_jmpbuf(png))) {
        LOG(ERROR) << "Error during end of write";
        png_destroy_write_struct(&png, &info);
        return false;
    }

    png_write_end(png, nullptr);

    for (int y = 0; y < height; y++) {
        free(row_data[y]);

        // TODO: memory leak if it errors above
    }

    png_destroy_write_struct(&png, &info);

    contains_data = false;
    return true;
}
